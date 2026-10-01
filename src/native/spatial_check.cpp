#include "assets.hpp"
#include "spatial.hpp"
#include "legacy.hpp"
#include "igc.h"
#include <cstdio>
#include <stdexcept>
int main(int argc,char** argv) {
    try {
        Native::Assets assets(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());
        UTL::SetArtPath(assets.root().string().c_str());
        size_t checked=0;
        for(const char* name:{"wc_icfig","wc_icint","wc_icsf","fig05","fig21","fig13","ss27","mis05","mis06","mis07"}) {
            auto model=assets.model(name);auto* hull=HitTest::Load(name);
            if(!hull)throw std::runtime_error(std::string("Missing collision hull: ")+name);
            float maxError=0;size_t matched=0;
            for(auto& frame:model->frames)if(auto* collision=hull->GetFrame(frame.name.c_str())) {
                // Compare the coordinate systems before each legacy subsystem's
                // radius normalization. Some authored CVH/MDL radii differ.
                auto rendered=glm::vec3(-frame.position.x,frame.position.y,-frame.position.z)/hull->GetOriginalRadius();
                auto point=collision->position/hull->GetOriginalRadius();
                maxError=std::max(maxError,glm::distance(rendered,{point.x,point.y,point.z}));++matched;
            }
            printf("%s: %zu attachment positions, normalized maximum error %.6f (mesh radius %.4f / CVH %.4f)\n",name,matched,maxError,model->radius,hull->GetOriginalRadius());
            if(maxError>.0001f)throw std::runtime_error("Artwork attachment axes disagree with collision geometry");
            checked+=matched;
        }
        if(checked<17)throw std::runtime_error("Insufficient collision/render attachment coverage");
        for(auto forward:{Vector(0,0,-1),Vector(1,0,0),Vector(.3f,.7f,-.4f)}) {
            forward.SetNormalize();Orientation o(forward);o.Roll(.8f);
            auto matrix=Native::modelTransform({o.GetRight().x,o.GetRight().y,o.GetRight().z},
                {o.GetUp().x,o.GetUp().y,o.GetUp().z},{o.GetBackward().x,o.GetBackward().y,o.GetBackward().z},{31,42,-13},2,true);
            Vector local(3,-5,7);auto expected=Vector(-local.x,local.y,-local.z)*o*2+Vector(31,42,-13);
            if(glm::distance(glm::vec3(matrix*glm::vec4(3,-5,7,1)),{expected.x,expected.y,expected.z})>1e-4f)
                throw std::runtime_error("Native model transform disagrees with original ThingGeo convention");
        }
        puts("Artwork, collision attachments and rotated model transforms agree");return 0;
    }catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}
}
