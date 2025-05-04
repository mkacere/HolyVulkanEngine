#version 450  
#extension GL_KHR_vulkan_glsl : enable  

layout(location = 0) in vec3 fragNormal;  
layout(location = 1) in vec3 fragColor;  
layout(location = 2) in vec2 fragUV;  

layout(set = 0, binding = 0) uniform GlobalUbo {  
   mat4 projection;  
   mat4 view;  
   mat4 inverseView;  
   vec4 ambientLightColor;  
   int   numLights;  
} ubo;  

layout(set = 0, binding = 1) uniform sampler2D baseColorTexture;  

layout(location = 0) out vec4 outColor;  

void main() {  
   // sample your GLTF base‐color image:  
   vec4 base = texture(baseColorTexture, fragUV);  

   // simple unlit pass‑through:  
   outColor = base;  

   // if you also want to tint by vertex color:  
   // outColor = base * vec4(fragColor, 1.0);  
}
