#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(set=0,binding=0) uniform sampler2D textures[];
layout(std430,set=1,binding=1) readonly buffer GammaTable {float levels[];};
layout(push_constant) uniform Ramp {
    layout(offset=112) uint textureID;
} ramp;
layout(location=0) out vec4 outColor;
void main() {
    // Apply WinTrek's display ramp AFTER all opaque/alpha/additive composition.
    // Fetch exact framebuffer texels; integer-scaled UI never gets resampled.
    vec3 value=texelFetch(textures[nonuniformEXT(ramp.textureID)],ivec2(gl_FragCoord.xy),0).rgb;
    uvec3 index=uvec3(round(clamp(value,0.0,1.0)*255.0));
    outColor=vec4(levels[index.r],levels[index.g],levels[index.b],1.0);
}
