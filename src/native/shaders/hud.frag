#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(set=0,binding=0) uniform sampler2D textures[];
layout(location=0) in vec2 uv;
layout(location=1) in vec4 color;
layout(location=2) flat in uint textureID;
layout(location=0) out vec4 outColor;
void main() {
    vec4 sampleColor=texture(textures[nonuniformEXT(textureID & 0x7fffffffu)],uv)*color;
    outColor=vec4(sampleColor.rgb*sampleColor.a,(textureID & 0x80000000u)!=0u ? 0.0 : sampleColor.a);
}
