#version 450

layout(location=0) in vec3 fragPosWorld;
layout(location=1) in vec3 fragNormalWorld;
layout(location=2) in vec3 fragColor;

layout(location=0) out vec4 outColor;

void main() {
    // simple directional light
    vec3 lightDir = normalize(vec3( 0.5,  1.0,  0.3)); // tweak direction as you like
    vec3 lightClr = vec3(1.0);

    // ambient + diffuse
    vec3 ambient = 0.1 * fragColor;
    float diff   = max(dot(fragNormalWorld, lightDir), 0.0);
    vec3 diffuse = diff * lightClr * fragColor;

    vec3 result  = ambient + diffuse;
    outColor     = vec4(result, 1.0);
}
