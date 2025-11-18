#version 450

// ============================================================================
// 3D Raymarched Fractal Fragment Shader
// ============================================================================

// ============================================================================
// Descriptors (Set 0: Global)
// ============================================================================

layout (set = 0, binding = 1) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invView;
    mat4 invProjection;
    vec4 position;
    vec4 direction;
    vec2 nearFar;
    vec2 screenSize;
    float fov;
    float aspectRatio;
    uint _pad0;
    uint _pad1;
} Camera;

// ============================================================================
// Push Constants (Fractal Parameters)
// ============================================================================

layout (push_constant) uniform FractalParams {
    // Fractal parameters
    uint fractalType;     // 0=Mandelbulb, 1=QuaternionJulia, 2=MengerSponge
    float power;          // Fractal power
    uint maxIterations;   // Escape iterations
    float bailout;        // Escape radius

    // Julia set parameters
    vec4 juliaC;          // Quaternion constant

    // Raymarching quality
    uint maxSteps;        // Max raymarch steps
    float epsilon;        // Surface hit threshold
    float maxDistance;    // Max ray distance
    float _pad0;

    // Lighting
    vec3 lightDir;        // Directional light
    float ambientStrength;
    vec3 lightColor;
    float aoStrength;

    // Coloring
    vec3 color1;
    float colorMix;
    vec3 color2;
    float _pad1;

    // Animation
    float time;
    uint enableAnimation;
    vec2 _pad2;

    // Psychedelic effects
    float glowIntensity;
    float colorCycleSpeed;
    float depthColorShift;
    float iterationColorMix;
} params;

// ============================================================================
// Output
// ============================================================================

layout (location = 0) out vec4 outColor;

// ============================================================================
// Color Utilities
// ============================================================================

// HSV to RGB conversion for rainbow effects
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// ============================================================================
// Distance Estimation Functions
// ============================================================================

// Mandelbulb distance estimation
float deMandelbulb(vec3 pos) {
    vec3 z = pos;
    float dr = 1.0;
    float r = 0.0;
    float power = params.power;

    for (uint i = 0; i < params.maxIterations; i++) {
        r = length(z);
        if (r > params.bailout) break;

        // Convert to polar coordinates
        float theta = acos(z.z / r);
        float phi = atan(z.y, z.x);
        dr = pow(r, power - 1.0) * power * dr + 1.0;

        // Scale and rotate the point
        float zr = pow(r, power);
        theta = theta * power;
        phi = phi * power;

        // Convert back to Cartesian coordinates
        z = zr * vec3(
            sin(theta) * cos(phi),
            sin(phi) * sin(theta),
            cos(theta)
        );
        z += pos;
    }

    return 0.5 * log(r) * r / dr;
}

// Quaternion Julia set distance estimation
float deQuaternionJulia(vec3 pos) {
    vec4 z = vec4(pos, 0.0);
    vec4 c = params.juliaC;
    float md2 = 1.0;
    float mz2 = dot(z, z);

    for (uint i = 0; i < params.maxIterations; i++) {
        md2 *= 4.0 * mz2;

        // Quaternion multiplication: z = z^2 + c
        z = vec4(
            z.x * z.x - z.y * z.y - z.z * z.z - z.w * z.w,
            2.0 * z.x * z.y,
            2.0 * z.x * z.z,
            2.0 * z.x * z.w
        ) + c;

        mz2 = dot(z, z);
        if (mz2 > params.bailout) break;
    }

    return 0.25 * sqrt(mz2 / md2) * log(mz2);
}

// Menger Sponge distance estimation (exact)
float deMengerSponge(vec3 pos) {
    float d = length(max(abs(pos) - vec3(1.0), 0.0)) - 0.05;
    float s = 1.0;

    for (uint i = 0; i < uint(params.power); i++) {
        vec3 a = mod(pos * s, 2.0) - 1.0;
        s *= 3.0;
        vec3 r = abs(1.0 - 3.0 * abs(a));

        float da = max(r.x, r.y);
        float db = max(r.y, r.z);
        float dc = max(r.z, r.x);
        float c = (min(da, min(db, dc)) - 1.0) / s;

        d = max(d, c);
    }

    return d;
}

// Main distance estimation function (selects fractal type)
float de(vec3 pos) {
    if (params.fractalType == 0) {
        return deMandelbulb(pos);
    } else if (params.fractalType == 1) {
        return deQuaternionJulia(pos);
    } else {
        return deMengerSponge(pos);
    }
}

// ============================================================================
// Normal Calculation (using gradient)
// ============================================================================

vec3 calculateNormal(vec3 pos) {
    const float h = 0.0001;
    const vec2 k = vec2(1.0, -1.0);
    return normalize(
        k.xyy * de(pos + k.xyy * h) +
        k.yyx * de(pos + k.yyx * h) +
        k.yxy * de(pos + k.yxy * h) +
        k.xxx * de(pos + k.xxx * h)
    );
}

// ============================================================================
// Ambient Occlusion
// ============================================================================

float calculateAO(vec3 pos, vec3 normal) {
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 5; i++) {
        float h = 0.01 + 0.12 * float(i) / 4.0;
        float d = de(pos + h * normal);
        occ += (h - d) * sca;
        sca *= 0.95;
    }
    return clamp(1.0 - 1.5 * occ, 0.0, 1.0);
}

// ============================================================================
// Raymarching
// ============================================================================

struct RayMarchResult {
    bool hit;
    float dist;
    vec3 pos;
    int steps;
};

RayMarchResult rayMarch(vec3 ro, vec3 rd) {
    RayMarchResult result;
    result.hit = false;
    result.dist = 0.0;
    result.steps = 0;

    float t = 0.0;

    for (uint i = 0; i < params.maxSteps; i++) {
        result.steps = int(i);
        vec3 pos = ro + rd * t;
        float d = de(pos);

        if (d < params.epsilon) {
            result.hit = true;
            result.pos = pos;
            result.dist = t;
            break;
        }

        t += d;

        if (t > params.maxDistance) {
            break;
        }
    }

    return result;
}

// ============================================================================
// Lighting
// ============================================================================

vec3 calculateLighting(vec3 pos, vec3 normal, vec3 viewDir, float ao) {
    // Normalize light direction
    vec3 L = normalize(-params.lightDir);
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    // Diffuse
    float NdotL = max(dot(normal, L), 0.0);
    vec3 diffuse = params.lightColor * NdotL;

    // Specular (Phong)
    float NdotH = max(dot(normal, H), 0.0);
    float spec = pow(NdotH, 32.0);
    vec3 specular = params.lightColor * spec * 0.5;

    // Ambient
    vec3 ambient = params.lightColor * params.ambientStrength;

    // Combine with AO
    vec3 lighting = ambient + (diffuse + specular) * ao;

    return lighting;
}

// ============================================================================
// Coloring
// ============================================================================

vec3 calculateColor(vec3 pos, float dist, int steps) {
    // === TRIPPY PSYCHEDELIC COLORING ===

    // 1. Iteration-based rainbow (like Mandelbrot zooms!)
    float iterationRatio = float(steps) / float(params.maxSteps);
    float hue = fract(iterationRatio * 10.0 + params.time * params.colorCycleSpeed);
    vec3 rainbowColor = hsv2rgb(vec3(hue, 0.8, 0.9));

    // 2. Depth-based hue shift (colors change as you zoom)
    float depthHue = fract(dist * params.depthColorShift + params.time * params.colorCycleSpeed * 0.3);
    vec3 depthColor = hsv2rgb(vec3(depthHue, 0.7, 0.8));

    // 3. Position-based variation (spatial patterns)
    float variation = sin(pos.x * 3.0) * cos(pos.y * 3.0) * sin(pos.z * 3.0);
    variation = variation * 0.5 + 0.5;

    // 4. Base color from user settings
    vec3 baseColor = mix(params.color1, params.color2, params.colorMix + variation * 0.2);

    // 5. Combine effects based on user preferences
    vec3 psychedelicColor = mix(depthColor, rainbowColor, 0.5);
    vec3 finalColor = mix(baseColor, psychedelicColor, params.iterationColorMix);

    // 6. Add pulsing based on time (if animation enabled)
    if (params.enableAnimation != 0u) {
        float pulse = sin(params.time * 2.0) * 0.1 + 0.9;
        finalColor *= pulse;
    }

    // 7. Enhance saturation for more trippy look
    float luminance = dot(finalColor, vec3(0.299, 0.587, 0.114));
    finalColor = mix(vec3(luminance), finalColor, 1.3); // Boost saturation

    return finalColor;
}

// ============================================================================
// Main
// ============================================================================

void main() {
    // Compute NDC from pixel coordinates
    vec2 ndc;
    ndc.x = (gl_FragCoord.x / Camera.screenSize.x) * 2.0 - 1.0;
    ndc.y = (gl_FragCoord.y / Camera.screenSize.y) * 2.0 - 1.0;

    // Reconstruct ray direction in world space
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewDir = Camera.invProjection * clip;
    viewDir /= viewDir.w;
    vec3 rayDir = normalize(mat3(Camera.invView) * viewDir.xyz);

    // Ray origin is camera position
    vec3 rayOrigin = Camera.position.xyz;

    // Raymarch
    RayMarchResult result = rayMarch(rayOrigin, rayDir);

    // Background color (dark blue/black gradient)
    vec3 backgroundColor = mix(
        vec3(0.01, 0.01, 0.02),
        vec3(0.05, 0.05, 0.1),
        abs(ndc.y)
    );

    if (result.hit) {
        // Calculate surface properties
        vec3 normal = calculateNormal(result.pos);
        float ao = mix(1.0, calculateAO(result.pos, normal), params.aoStrength);

        // Calculate base color (with psychedelic effects!)
        vec3 baseColor = calculateColor(result.pos, result.dist, result.steps);

        // Calculate lighting
        vec3 lighting = calculateLighting(result.pos, normal, -rayDir, ao);

        // Combine color and lighting
        vec3 finalColor = baseColor * lighting;

        // === EDGE GLOW EFFECT ===
        // Fresnel-like rim lighting for trippy glow
        float fresnel = pow(1.0 - abs(dot(normal, -rayDir)), 3.0);
        vec3 glowColor = hsv2rgb(vec3(fract(params.time * params.colorCycleSpeed * 0.5), 1.0, 1.0));
        finalColor += glowColor * fresnel * params.glowIntensity;

        // Distance fog
        float fogFactor = smoothstep(params.maxDistance * 0.5, params.maxDistance, result.dist);
        finalColor = mix(finalColor, backgroundColor, fogFactor);

        // Gamma correction
        finalColor = pow(finalColor, vec3(1.0 / 2.2));

        outColor = vec4(finalColor, 1.0);
    } else {
        // No hit - output background
        outColor = vec4(backgroundColor, 1.0);
    }
}
