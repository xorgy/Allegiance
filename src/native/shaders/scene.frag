#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(set=0,binding=0) uniform sampler2D textures[];
layout(location=0) in vec2 uv;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;
layout(location=3) flat in uvec2 material;
layout(location=0) out vec4 outColor;
void main() {
    vec4 texel=texture(textures[nonuniformEXT(material.x)],uv)*color;
    if((material.y&2u)==0u && texel.a<0.15) discard;
    outColor=(material.y&2u)!=0u?vec4(texel.rgb*texel.a,(material.y&4u)!=0u?0:texel.a):vec4(texel.rgb,texel.a);
}
