#version 460
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
struct Instance { mat4 model;vec4 color;uvec4 params; };
layout(set=1,binding=0,std430) readonly buffer Instances { Instance instances[]; };
layout(push_constant) uniform Camera { mat4 viewProjection;vec4 lightDirection;vec4 primary;vec4 alternate; } camera;
layout(location=0) out vec2 outUV;
layout(location=1) out vec3 outNormal;
layout(location=2) out vec4 outColor;
layout(location=3) flat out uvec2 material;
void main() {
    Instance object=instances[gl_InstanceIndex];
    gl_Position=camera.viewProjection*object.model*vec4(position,1);
    if((object.params.y&32u)!=0u)gl_Position.z=0; // Backdrops are infinitely distant and do not write depth.
    outUV=uv;if((object.params.y&8u)!=0u)outUV.x=uv.x*uintBitsToFloat(object.params.w)+uintBitsToFloat(object.params.z);outNormal=mat3(object.model)*normal;outColor=object.color;
    if((object.params.y&16u)!=0u){outUV.y+=uintBitsToFloat(object.params.z);outColor.rgb*=normal.x;}
    if((object.params.y&1u)==0u) {
        float incident=dot(normalize(outNormal),camera.lightDirection.xyz);
        outColor.rgb*=clamp(vec3(camera.lightDirection.w)+camera.primary.rgb*max(-incident,0.0)+camera.alternate.rgb*max(incident,0.0),0.0,1.0);
    }
    material=object.params.xy;
}
