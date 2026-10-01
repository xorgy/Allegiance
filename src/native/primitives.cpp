#include "primitives.hpp"
#include "../ClientCore/AlephEffects.h"
#include <glm/gtc/constants.hpp>
#include <random>
namespace Native {
std::shared_ptr<ModelData> makeProjectile() {
    auto mesh=std::make_shared<MeshData>();
    for(auto p:{glm::vec3(1,0,0),{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})mesh->vertices.push_back({p,p,{0,0}});
    mesh->indices={0,2,4,2,1,4,1,3,4,3,0,4,2,0,5,1,2,5,3,1,5,0,3,5};
    auto model=std::make_shared<ModelData>();model->meshes.push_back(mesh);return model;
}
std::shared_ptr<ModelData> makeWarp(int detail) {
    constexpr int depths[]={8,4,2,2},sections[]={32,16,16,8};
    int depth=depths[detail],section=sections[detail];auto mesh=std::make_shared<MeshData>();
    mesh->vertices.reserve((depth+1)*(section+1));mesh->indices.reserve(depth*section*6);
    for(int d=0;d<=depth;++d)for(int s=0;s<=section;++s) {
        auto p=Allegiance::AlephPoint(float(d)/depth,float(s)/section);
        // Aleph vertices are unlit: reuse normal.x for the original brightness.
        mesh->vertices.push_back({{p.x,p.y,p.z},{p.brightness,0,1},{p.u,p.v}});
    }
    for(int d=0;d<depth;++d)for(int s=0;s<section;++s) {
        uint32_t a=d*(section+1)+s,b=a+section+1;
        mesh->indices.insert(mesh->indices.end(),{a,b,b+1,a,b+1,a+1});
    }
    auto model=std::make_shared<ModelData>();model->meshes.push_back(mesh);return model;
}
std::shared_ptr<ModelData> makeStars() {
    auto mesh=std::make_shared<MeshData>();std::mt19937 rng(547);std::uniform_real_distribution<float> random(-1,1);
    for(int i=0;i<1400;++i) {
        glm::vec3 p(random(rng),random(rng),random(rng));if(glm::length(p)<0.01f){--i;continue;}p=glm::normalize(p);
        auto x=glm::normalize(glm::cross(p,glm::vec3(0,1,0)))*0.00022f;auto y=glm::normalize(glm::cross(p,x))*0.00022f;
        uint32_t base=mesh->vertices.size();for(auto v:{p-x-y,p+x-y,p+y})mesh->vertices.push_back({v*9000.0f,-p,{0,0}});
        mesh->indices.insert(mesh->indices.end(),{base,base+1,base+2});
    }
    auto model=std::make_shared<ModelData>();model->radius=9001;model->meshes.push_back(mesh);return model;
}
}
