#version 460
layout(location=0) in vec4 centerSize;
layout(location=1) in vec4 axisWidth;
layout(location=2) in vec4 color;
layout(location=3) in vec4 uvRect;
layout(location=4) in uint textureIndex;
layout(location=5) in float angle;
layout(push_constant) uniform Camera {mat4 viewProjection;vec4 right;vec4 up;} camera;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 tint;
layout(location=2) flat out uint textureID;
void main() {
    const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
    vec2 corner=corners[gl_VertexIndex],q=corner*2-1;
    if((textureIndex&0x10000000u)!=0u) {
        gl_Position=camera.viewProjection*vec4(centerSize.xyz+uvRect.xyz*q.x-axisWidth.xyz*q.y,1);
        uv=corner;tint=color;textureID=textureIndex&~0x10000000u;return;
    }
    if((textureIndex&0x40000000u)!=0u) {
        vec3 right=mix(uvRect.xyz,vec3(centerSize.w,axisWidth.w,angle),corner.y);
        gl_Position=camera.viewProjection*vec4(centerSize.xyz+axisWidth.xyz*q.y+right*q.x,1);
        uv=vec2(corner.x,0);tint=vec4(color.rgb*mix(uvRect.w,1,corner.y),color.a);textureID=textureIndex&~0x40000000u;return;
    }
    if((textureIndex&0x20000000u)!=0u) {
        vec4 a=camera.viewProjection*vec4(centerSize.xyz,1),b=camera.viewProjection*vec4(axisWidth.xyz,1);
        if(a.w<=0 && b.w<=0){gl_Position=vec4(2,2,2,1);uv=vec2(0);tint=vec4(0);textureID=0;return;}
        if(a.w<.001)a=mix(a,b,(.001-a.w)/(b.w-a.w));
        if(b.w<.001)b=mix(b,a,(.001-b.w)/(a.w-b.w));
        vec2 viewport=vec2(camera.right.w,camera.up.w);
        vec2 delta=(b.xy/b.w-a.xy/a.w)*viewport;
        vec2 normal=dot(delta,delta)>0?normalize(vec2(-delta.y,delta.x)):vec2(1,0);
        gl_Position=mix(a,b,corner.y);
        gl_Position.xy+=normal*(centerSize.w/viewport)*q.x*gl_Position.w;
        uv=vec2(mix(uvRect.x,uvRect.z,corner.y),0);tint=color;textureID=textureIndex&~0x20000000u;return;
    }
    vec3 x,y;
    if(dot(axisWidth.xyz,axisWidth.xyz)>0.000001) {
        y=axisWidth.xyz;
        x=cross(cross(camera.right.xyz,camera.up.xyz),y);
        x=dot(x,x)>0.000001?normalize(x):camera.right.xyz;
        x*=centerSize.w*axisWidth.w;
    } else {
        float c=cos(angle),s=sin(angle);
        x=(camera.right.xyz*c+camera.up.xyz*s)*centerSize.w;
        y=(-camera.right.xyz*s+camera.up.xyz*c)*centerSize.w;
    }
    gl_Position=camera.viewProjection*vec4(centerSize.xyz+x*q.x-y*q.y,1);
    uv=mix(uvRect.xy,uvRect.zw,corner);tint=color;textureID=textureIndex;
}
