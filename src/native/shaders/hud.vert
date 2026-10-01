#version 460
layout(location=0) in vec4 bounds;
layout(location=1) in vec4 uv;
layout(location=2) in vec4 color;
layout(location=3) in uint textureID;
layout(location=0) out vec2 outUV;
layout(location=1) out vec4 outColor;
layout(location=2) flat out uint outTexture;
void main() {
    const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
    vec2 p=corners[gl_VertexIndex];
    bool oriented=(textureID & 0x20000000u)!=0u;
    bool line=(textureID & 0x60000000u)!=0u;
    vec2 position=line ? mix(bounds.xy,bounds.zw,p.x)+uv.xy*(p.y*2-1) : mix(bounds.xy,bounds.zw,p);
    gl_Position=vec4(position,0,1);
    outUV=oriented ? p : line ? vec2(0.5) : mix(uv.xy,uv.zw,p);outColor=color;outTexture=textureID & ~0x60000000u;
}
