// Real Vulkan compositing checks. Run explicitly on a Wayland/Vulkan machine;
// this is separate from the headless simulation CTest suite.
#include "renderer.hpp"
#include "hud.hpp"
#include "game.hpp"
#include "settings.hpp"
#include "frontend.hpp"
#include "crew_pane.hpp"
#include "console_pane.hpp"
#include "game_state_pane.hpp"
#include "player_pane.hpp"
#include "original_cursor.hpp"
#include "training_slides.hpp"
#include "test_reference/authored_hosts.hpp"
#include "../ClientCore/TrainingPresentation.h"
#include "../Igc/igc.h"
#include "../ClientCore/SectorMapPresentation.h"
#include "../ClientCore/Overlay.h"
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <chrono>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include <stdexcept>
using namespace Native;
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::array<int,3> pixel(const std::filesystem::path& path,int x=-1,int y=-1) {
    std::ifstream in(path,std::ios::binary);std::string magic;int w,h,maximum;in>>magic>>w>>h>>maximum;in.get();
    require(magic=="P6" && w>0 && h>0 && maximum==255,"invalid screenshot");
    if(x<0)x=w/2;if(y<0)y=h/2;
    require(x<w && y<h,"pixel outside screenshot");
    in.seekg(std::streamoff((size_t(y)*w+x)*3),std::ios::cur);std::array<unsigned char,3> result{};in.read(reinterpret_cast<char*>(result.data()),3);
    require(bool(in),"truncated screenshot");return {result[0],result[1],result[2]};
}
}
int main(int argc,char** argv) {
    try {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());struct SdlCleanup{~SdlCleanup(){SDL_Quit();}} cleanup;
        configureVulkanPlatform();
        auto window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>(SDL_CreateWindow("Allegiance compositing checks",1000,700,SDL_WINDOW_VULKAN|SDL_WINDOW_BORDERLESS),SDL_DestroyWindow);
        require(bool(window),SDL_GetError());Renderer renderer(window.get(),!(argc>2 && std::string_view(argv[2])=="--no-validation"));
        auto image=std::make_shared<ImageData>();image->width=image->height=1;image->mipmaps=false;image->pixels={255,255,255,255};
        auto mesh=std::make_shared<MeshData>();mesh->image=image;
        mesh->vertices={{{-1,-1,0},{0,0,1},{0,0}},{{1,-1,0},{0,0,1},{1,0}},{{1,1,0},{0,0,1},{1,1}},{{-1,1,0},{0,0,1},{0,1}}};mesh->indices={0,1,2,0,2,3};
        auto model=std::make_shared<ModelData>();model->radius=2;model->meshes.push_back(mesh);uint32_t id=renderer.addModel(model),texture=renderer.addTexture(image);renderer.flushUploads();
        auto projection=glm::orthoRH_ZO(-2.f,2.f,-2.f,2.f,10.f,.01f);projection[1][1]*=-1;
        const auto file=std::filesystem::temp_directory_path()/("allegiance-compositing-"+std::to_string(getpid())+".ppm");
        auto render=[&](std::span<const DrawItem> items,std::span<const ParticleDraw> particles,std::span<const HudQuad> hud=std::span<const HudQuad>{},HudBackground sky={},const ModelPreview* preview=nullptr) {
            for(int retry=0;!renderer.beginFrame();++retry){SDL_Event e;while(SDL_PollEvent(&e)){};require(retry<500,"could not acquire a Wayland frame");SDL_Delay(2);}
            renderer.draw(projection,items,hud,file.string(),preview,particles,{},sky);return pixel(file);
        };
        auto before=renderer.resources();auto skin=std::make_shared<ImageData>(*image);skin->pixels={0,255,0,255};
        uint32_t greenSkin=renderer.addModel(model,skin);auto alias=std::make_shared<ModelData>(*model);renderer.addModel(alias,skin);renderer.flushUploads();
        auto after=renderer.resources();require(after.vertices==before.vertices && after.indices==before.indices && after.textures==before.textures+1,"skin variants duplicate geometry or textures");
        require(renderer.addModel(model,skin)==greenSkin && renderer.addModel(model)==id,"material/model lookup does not reuse its ID");
        DrawItem skinDraw{greenSkin,glm::translate(glm::mat4(1),{0,0,-2}),glm::vec4(1),true};
        auto material=render({&skinDraw,1},{});require(material[0]<10 && material[1]>250 && material[2]<10,"shared geometry lost its skin material");
        {
            auto ramp=std::make_shared<ImageData>();ramp->width=256;ramp->height=1;ramp->mipmaps=false;ramp->pixels.resize(256*4);
            for(int x=0;x<256;++x){ramp->pixels[4*x]=x;ramp->pixels[4*x+1]=255-x;ramp->pixels[4*x+2]=x/2;ramp->pixels[4*x+3]=255;}
            auto rampTexture=renderer.addTexture(ramp,true);renderer.flushUploads();auto size=renderer.size();
            HudQuad strip{{-1,-1,1,1},{0,0,1,1},glm::vec4(1),rampTexture};
            auto resources=renderer.resources();
            for(float gamma:{1.f,1.13f,2.f,1.01f,1.f}){
                renderer.gamma(gamma);render({},{},{&strip,1});
                for(int x=0;x<256;++x){int px=int(std::ceil((x+.5)*size.x/256.));auto color=pixel(file,px,size.y/2);
                    for(int c=0;c<3;++c){int expected=int(std::lround(255*std::pow(ramp->pixels[4*x+c]/255.,1/gamma)));require(std::abs(color[c]-expected)<=1,"final display ramp disagrees with original encoded gamma or resamples UI texels");}}
            }
            HudQuad layers[]={{{-1,-1,1,1},{0,0,1,1},{.1f,.2f,.05f,1},texture},{{-1,-1,1,1},{0,0,1,1},{.3f,.4f,.5f,.4f},texture},{{-1,-1,1,1},{0,0,1,1},{.1f,.2f,.3f,.3f},texture|hudAdditive}};
            auto neutral=render({},{},layers);renderer.gamma(1.6f);auto bright=render({},{},layers);
            for(int c=0;c<3;++c)require(std::abs(bright[c]-int(std::lround(255*std::pow(neutral[c]/255.,1/1.6))))<=1,"gamma is applied before alpha/additive composition");
            renderer.gamma(1);require(render({},{},layers)==neutral,"neutral gamma does not restore the direct rendering path");
            require(renderer.resources().textures==resources.textures,"brightness changes duplicate resident asset textures");
            auto info=renderer.deviceInfo();require(info.size()==5 && info[0].starts_with("Device: ") && info[2]=="Resolution: "+std::to_string(size.x)+" x "+std::to_string(size.y),"graphics menu does not report actual selected Vulkan device/extent");
            puts("Display gamma pixels: all 256 encoded values, original 1.13 startup, bounded ramp, exact texels, alpha/additive final composition, frame reuse and neutral bypass passed");
        }
        {
            auto options=renderer.antialiasingOptions();require(!options.empty() && options.front()==0 && std::is_sorted(options.begin(),options.end()),"AA menu does not expose ordered supported sample counts");
            if(options.size()>1){
                int samples=options.back();if(samples>4)samples=*(std::upper_bound(options.begin(),options.end(),4)-1);
                auto partial=[&]{std::ifstream in(file,std::ios::binary);std::string magic;int w,h,max;in>>magic>>w>>h>>max;in.get();std::vector<unsigned char> rgb(size_t(w)*h*3);in.read(reinterpret_cast<char*>(rgb.data()),rgb.size());require(bool(in),"truncated AA readback");size_t count=0;for(size_t i=0;i<rgb.size();i+=3)if(rgb[i]>60 && rgb[i]<240 && std::abs(int(rgb[i])-rgb[i+1])<3 && std::abs(int(rgb[i])-rgb[i+2])<3)++count;return count;};
                DrawItem edge{id,glm::translate(glm::mat4(1),{0,0,-2})*glm::rotate(glm::mat4(1),.31f,{0,0,1}),glm::vec4(1),true};
                render({&edge,1},{});auto off=partial();auto resources=renderer.resources();require(renderer.antialiasing(samples)==samples,"AA selects an unsupported sample count");render({&edge,1},{});require(partial()>off+100,"multisampling does not resolve partial geometry coverage");
                HudQuad exact{{-1,-1,1,1},{0,0,1,1},{.2f,.3f,.4f,1},texture};auto neutral=render({},{},{&exact,1});renderer.gamma(1.13f);auto bright=render({},{},{&exact,1});
                for(int c=0;c<3;++c)require(std::abs(bright[c]-int(std::lround(255*std::pow(neutral[c]/255.,1/1.13))))<=1,"gamma does not follow multisample resolve");
                renderer.gamma(1);ModelPreview preview{skinDraw,projection,{0,0,renderer.size().x,renderer.size().y}};auto color=render({&edge,1},{},{},{},&preview);require(color[0]<10 && color[1]>250 && color[2]<10,"AA loses depth-cleared model previews");
                DrawItem alpha{id,glm::translate(glm::mat4(1),{0,0,-2}),{1,0,0,.5f},true,ModelBlend::Alpha};ParticleDraw add{{0,0,-4,1},{0,0,0,1},{0,0,1,1},{0,0,1,1},texture|hudAdditive};auto behind=render({&alpha,1},{&add,1});add.centerSize.z=-1;auto front=render({&alpha,1},{&add,1});require(front[2]>behind[2]+40 && std::abs(front[0]-behind[0])<3,"AA changes alpha/additive depth order");
                renderer.antialiasing(0);require(render({},{},{&exact,1})==neutral,"AA off does not restore exact UI colors");require(renderer.resources().textures==resources.textures,"AA mode changes duplicate resident assets");
                printf("Multisample pixels: %dx hardware edge resolve, gamma ordering, alpha/additive depth, model previews, exact UI/off restoration and retained assets passed\n",samples);
            }
        }
        {
            auto checker=std::make_shared<ImageData>();checker->width=checker->height=512;checker->pixels.resize(512*512*4);
            for(int y=0;y<512;++y)for(int x=0;x<512;++x){auto offset=(size_t(y)*512+x)*4;bool red=(x+y)&1;checker->pixels[offset]=red?255:0;checker->pixels[offset+1]=0;checker->pixels[offset+2]=red?0:255;checker->pixels[offset+3]=255;}
            auto checkerMesh=std::make_shared<MeshData>(*mesh);checkerMesh->image=checker;auto checkerModel=std::make_shared<ModelData>(*model);checkerModel->meshes={checkerMesh};auto checkerId=renderer.addModel(checkerModel);auto nearest=renderer.addTexture(checker,true);renderer.flushUploads();auto resources=renderer.resources();
            DrawItem full{checkerId,glm::translate(glm::mat4(1),{0,0,-2})*glm::scale(glm::mat4(1),{2,2,1}),glm::vec4(1),true};
            auto contrast=[&]{int range=0;auto size=renderer.size();for(int y=30;y<size.y-30;y+=23)for(int x=30;x<size.x-30;x+=19){auto c=pixel(file,x,y);range=std::max(range,std::abs(c[0]-c[2]));}return range;};
            renderer.textureLimit(1);render({&full,1},{});auto fine=contrast();require(fine>50,"texture limit test has no original high-frequency detail");
            renderer.textureLimit(0);render({&full,1},{});require(contrast()<4,"256 texture limit does not select the resident reduced mip view");
            renderer.textureLimit(3);render({&full,1},{});require(contrast()==fine,"texture quality does not restore the original full mip chain");
            auto size=renderer.size();HudQuad ui{{-1,-1,-1+1024.f/size.x,-1+1024.f/size.y},{0,0,1,1},glm::vec4(1),nearest};
            for(int level:{0,1,2,3,0}){renderer.textureLimit(level);render({},{},{&ui,1});auto first=pixel(file,12,12),second=pixel(file,13,12);require(first[2]>250 && first[0]<4 && second[0]>250 && second[2]<4,"world texture limit reduces or filters original one-to-one UI texels");}
            renderer.textureLimit(3);require(renderer.addTexture(checker,true)==nearest && renderer.resources().textures==resources.textures,"texture quality changes duplicate resident images/descriptors");
            puts("Texture limit pixels: original 256/512/1024/2048 levels, reduced world mip detail, restored quality, shared one-to-one nearest UI and resident image reuse passed");
        }
        {
            std::vector<DrawItem> fleet;for(int i=0;i<2048;++i)fleet.push_back({i%2?greenSkin:id,glm::translate(glm::mat4(1),{0,0,-2.f-float(i%7)*.01f}),i%2?glm::vec4(1):glm::vec4(1,0,0,1),true});
            render(fleet,{});require(renderer.stats().draws==1 && renderer.stats().triangles==4096,"identical mesh/skin instances are not batched or dropped");
            auto nearest=pixel(file);require(nearest[0]>250 && nearest[1]<10,"instancing changes equal-depth submission order");
            fleet.resize(20000,fleet.front());std::vector<ParticleDraw> effects(20000,{{0,0,-3,1},{0,0,0,1},{0,0,1,.001f},{0,0,1,1},texture|hudAdditive});
            render(fleet,effects);require(renderer.stats().draws==2 && renderer.stats().triangles==80000,"grown instance/particle arenas drop draws");
            render(fleet,effects);render(fleet,effects); // Grow both reusable frames and reuse each descriptor.
        }
        {
            // Offscreen sprites must not consume GPU instances. These probes
            // also catch center-only rejection of a visible rotated/stretched
            // quad at a frustum edge, for both depth-tested particle pipelines.
            std::vector<ParticleDraw> effects(20000,{{100,100,-3,1},{0,0,0,1},{0,0,1,1},{0,0,1,1},texture|hudAdditive});
            render({},effects);require(renderer.stats().draws==0 && renderer.stats().triangles==0,"offscreen sprites still render");
            for(uint32_t blend:{0u,hudAdditive}) {
                ParticleDraw edge{{2.4f,0,-3,1},{0,0,0,1},{0,0,1,1},{0,0,1,1},texture|blend,.7853982f};
                render({},{&edge,1});require(renderer.stats().triangles==2 && pixel(file,renderer.size().x-5,renderer.size().y/2)[2]>250,"rotated billboard crossing frustum edge was culled");
                edge.centerSize={0,2.4f,-3,1};edge.axisWidth={0,2,0,.5f};
                render({},{&edge,1});require(renderer.stats().triangles==2 && pixel(file,renderer.size().x/2,5)[2]>250,"stretched projectile crossing frustum edge was culled");
            }
        }
        DrawItem red{id,glm::translate(glm::mat4(1),{0,0,-2}),{1,0,0,.5f},true,ModelBlend::Alpha};
        ParticleDraw blue{{0,0,-4,1},{0,0,0,1},{0,0,1,1},{0,0,1,1},texture|hudAdditive};
        auto behind=render({&red,1},{&blue,1});blue.centerSize.z=-1;auto front=render({&red,1},{&blue,1});
        require(behind[0]>100 && behind[2]>100 && behind[1]<10,"alpha model or additive particle missing");
        require(front[2]>behind[2]+40 && std::abs(front[0]-behind[0])<3,"nearer alpha model must attenuate a farther additive particle");
        ParticleDraw line{{-.75f,0,-1,20},{.75f,0,-5,0},{0,0,1,1},{0,0,1,0},texture|hudAdditive|particleLine};
        auto lineBehind=render({&red,1},{&line,1});require(std::abs(lineBehind[2]-behind[2])<3,"line ordering uses its first endpoint instead of its center");
        DrawItem green{id,glm::translate(glm::mat4(1),{0,0,-3}),{0,1,0,.5f},true,ModelBlend::Alpha};
        std::array<DrawItem,2> layers{red,green};blue.centerSize.z=-4;auto ordered=render(layers,{&blue,1});
        std::swap(layers[0],layers[1]);auto shuffled=render(layers,{&blue,1});require(ordered==shuffled,"transparency depends on submission order");
        require(ordered[0]>ordered[1]+25 && ordered[1]>100 && ordered[2]>100,"two alpha layers compose in the wrong depth order");
        DrawItem opaque=red;opaque.blend=ModelBlend::Opaque;opaque.color={1,0,0,1};auto hidden=render({&opaque,1},{&blue,1});
        require(hidden[0]>250 && hidden[1]<10 && hidden[2]<10,"opaque reverse-Z depth fails to occlude a particle");
        DrawItem add=red;add.blend=ModelBlend::Additive;add.color={1,0,0,1};auto pureAdd=render({&add,1},{&blue,1});
        require(pureAdd[0]>250 && pureAdd[2]>250 && pureAdd[1]<15,"pure-additive batching changed compositing");
        Assets assets(argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork());
        {
            auto capture=[&] {std::ifstream in(file,std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(in),{});};
            for(const char* name:{"bgrnd03","bgrnd05","bgrnd51","bgrnd52","bgrnd53","bgrnd55","bgrnd56"}) {
                auto rock=assets.model(name);auto authored=assets.image(std::string(name)+"bmp");
                uint32_t inherited=renderer.addModel(rock),explicitTexture=renderer.addModel(rock,authored),white=renderer.addModel(rock,image);renderer.flushUploads();
                for(float radius:{80.f,48.f,16.f,4.f}) {
                    // Orthographic projection makes the chosen projected
                    // radius exact; 16 px exercises the bare middle LOD.
                    float scale=radius/(rock->radius*renderer.size().x*.25f);
                    DrawItem asteroid{inherited,glm::scale(glm::translate(glm::mat4(1),{0,0,-2}),glm::vec3(scale)),glm::vec4(1),true};
                    render({&asteroid,1},{});auto actual=capture();
                    asteroid.model=explicitTexture;render({&asteroid,1},{});
                    require(actual==capture(),"asteroid LOD lost the inherited authored texture");
                    if(radius==16) {
                        asteroid.model=white;render({&asteroid,1},{});
                        require(actual!=capture(),"asteroid texture check has no visible textured geometry");
                    }
                }
            }
        }
        DrawItem backdrop=opaque;backdrop.color={.05f,.22f,.75f,1};auto background=render({&backdrop,1},{});
        for(const char* name:{"lockedbmp","plockbmp","targetedbmp","lastfiredbmp","hasflagbmp","hasartifactbmp"}) {
            auto source=assets.image(name),keyed=assets.image(name,true);
            require(assets.image(name,true)==keyed && assets.image(name)==source,"color-key variants are not cached independently");
            size_t blacks=0,visible=0,brightest=0;int brightness=-1;
            for(size_t i=0;i<source->pixels.size();i+=4) {
                const auto* a=source->pixels.data()+i;const auto* b=keyed->pixels.data()+i;
                require(a[0]==b[0] && a[1]==b[1] && a[2]==b[2],"color key changed authored RGB");
                if(!(a[0]|a[1]|a[2])){require(b[3]==0,"radar bracket black background is opaque");++blacks;}
                else {require(a[3]==b[3],"color key changed nonblack coverage");++visible;
                    int value=a[0]+a[1]+a[2];if(value>brightness){brightness=value;brightest=i/4;}}
            }
            require(blacks && visible,"radar bracket fixture lacks background or visible strokes");
            auto center=(size_t(source->height/2)*source->width+source->width/2)*4;
            require(keyed->pixels[center+3]==0,"radar bracket center is not transparent");
            require(source->pixels[center+3]==255,"keying mutated the unkeyed texture");
            uint32_t radarTexture=renderer.addTexture(keyed);renderer.flushUploads();
            for(int factor:{1,2}) {
                auto size=renderer.size();int w=int(source->width)*factor,h=int(source->height)*factor;
                int x=(size.x-w)/2,y=(size.y-h)/2;
                HudQuad bracket{{2.f*x/size.x-1,2.f*y/size.y-1,2.f*(x+w)/size.x-1,2.f*(y+h)/size.y-1},{0,0,1,1},glm::vec4(1),radarTexture};
                auto result=render({&backdrop,1},{},{&bracket,1});
                require(result==background,"radar bracket obscures the world at its transparent center");
                auto stroke=pixel(file,x+int(brightest%source->width)*factor+factor/2,y+int(brightest/source->width)*factor+factor/2);
                require(std::abs(stroke[0]-background[0])+std::abs(stroke[1]-background[1])+std::abs(stroke[2]-background[2])>30,"radar bracket stroke disappeared");
            }
        }
        Hud hud(renderer,assets);Settings settings;settings.visual.lensFlare=false;hud.configure(settings);renderer.flushUploads();
        {
            auto slides=loadTrainingSlides(assets,1,true);hud.preloadSlides(slides);auto font=assets.font("largeFont");
            auto fontTexture=renderer.addTexture(font->coverage,true);auto before=renderer.resources();
            auto quads=hud.buildSlide(renderer.size(),slides.front(),0,slides.size());renderer.flushUploads();render({},{},quads);
            auto glyph=std::find_if(quads.begin(),quads.end(),[&](const auto& q){return q.texture==fontTexture;});
            require(glyph!=quads.end() && std::count_if(quads.begin(),quads.end(),[&](const auto& q){return q.texture==fontTexture;})>80,"training text uses a substituted font or blend mode");
            auto layout=originalMenuLayout(renderer.size());auto w=font->glyphs['W'];auto lo=(glm::vec2(glyph->bounds)+1.f)*.5f*glm::vec2(renderer.size());auto hi=(glm::vec2(glyph->bounds.z,glyph->bounds.w)+1.f)*.5f*glm::vec2(renderer.size());
            require(glm::length(lo-layout.pixel({150,150}))<.001f && glm::length((hi-lo)-glm::vec2(w.z,w.w))<.001f,"training font is oversized or misplaced relative to original artwork");
            size_t probes=0;
            for(int y=0;y<int(w.w);++y)for(int x=0;x<int(w.z);++x){auto offset=size_t(w.y+y)*font->coverage->width+int(w.x)+x;
                auto alpha=font->coverage->pixels[offset];if(alpha==255){auto p=pixel(file,int(std::round(lo.x))+x,int(std::round(lo.y))+y);require(p[0]>250 && p[1]>250 && p[2]>250,"original training glyph coverage is absent from Vulkan pixels");++probes;}}
            require(probes>5,"training glyph pixel probe misses opaque coverage");
            auto circleSlides=loadTrainingSlides(assets,2,false);hud.preloadSlides(circleSlides);auto keyed=renderer.addTexture(assets.image("tm_slide_circle_overlaybmp",true),true);
            quads=hud.buildSlide(renderer.size(),circleSlides.front(),0,circleSlides.size());require(std::any_of(quads.begin(),quads.end(),[&](const auto& q){return q.texture==keyed;}),"training circle uses additive blending or loses its authored color key");
            renderer.flushUploads();render({},{},quads);auto stable=renderer.resources();for(int i=0;i<3;++i){hud.buildSlide(renderer.size(),slides.front(),0,slides.size());hud.buildSlide(renderer.size(),circleSlides.front(),0,circleSlides.size());}
            require(renderer.resources().textures==stable.textures && stable.textures>=before.textures,"steady slideshow rendering allocates new resident textures");
            puts("Original training pixels: authored largeFont, exact first-glyph placement/size/coverage, color-keyed alpha circle, shared image painter and resident texture reuse passed");
        }
        {
            MenuView menu;menu.original=true;menu.artwork.push_back({OriginalControl::Kind::Rectangle,{},{},{},{0,0,800,600},{0,.25f,.5f,1}});
            render({},{},hud.buildMenu(renderer.size(),menu,{}));auto background=pixel(file);
            auto image=std::make_shared<ImageData>();image->width=image->height=1;image->mipmaps=false;image->pixels={128,0,0,255};
            OriginalControl overlay{OriginalControl::Kind::Image,{},{},{},{300,200,200,200},glm::vec4(1),image};overlay.additive=true;menu.artwork.push_back(overlay);
            auto quads=hud.buildMenu(renderer.size(),menu,{});renderer.flushUploads();auto added=render({},{},quads);
            require(added[0]>background[0]+50 && std::abs(added[1]-background[1])<=1 && std::abs(added[2]-background[2])<=1,"authored additive image overwrites or darkens the underlying artwork");
            menu.artwork.back()={OriginalControl::Kind::Rectangle,{},{},{},{300,200,200,200},{.5f,0,0,.5f}};menu.artwork.back().additive=true;
            added=render({},{},hud.buildMenu(renderer.size(),menu,{}));
            require(added[0]>background[0]+50 && std::abs(added[1]-background[1])<=1 && std::abs(added[2]-background[2])<=1,"authored additive rectangle overwrites or darkens the underlying artwork");
            puts("Original BlendImage pixels: image and rectangle controls preserve the underlying artwork with additive blending passed");
        }
        {
            Hud styles(renderer,assets);Settings options;options.visual.lensFlare=false;
            GameState state;state.ship="Scout";state.sourcePilot="Pilot";state.sector="Wasp";state.playerSide=0;state.sideColors[0]={0,1,0,1};
            state.hull=.65f;state.shield=.8f;state.energy=.7f;state.fuel=.55f;state.ammoFraction=.4f;state.throttle=.9f;state.speed=217;
            state.signature=82;state.experience=15;state.coast=state.autopilot=state.cloaked=true;state.cloakingPercent=45;state.carriedFlag=-2;
            state.lives=3;state.killCounter=true;state.kills=7;
            state.target="Enemy fighter";state.targetType="Fighter";state.targetSector="Wasp";state.targetObjectType=OT_ship;state.targetSide=1;
            state.targetColor={1,0,0,1};state.targetDistance=1200;state.targetSpeed=165;state.targetHull=.6f;state.targetShield=.5f;state.targetExperience=8;
            state.targetHasShield=state.targetHasModel=state.inRange=true;state.targetModel={id,2};state.targetAppearance={id,glm::mat4(1),glm::vec4(1),true};
            EquipmentState mounting{};mounting.kind=PartKind::Weapon;mounting.name="Pulse laser";mounting.mount=0;mounting.fixedWeapon=mounting.selected=true;mounting.mounted=.5f;mounting.readiness=1;
            EquipmentState arming{};arming.kind=PartKind::Missile;arming.name="Missile";arming.mount=0;arming.amount=10;arming.ready=.5f;arming.readiness=2;
            state.equipment={mounting,arming};auto size=renderer.size();
            InventoryRow gun;gun.mdl="invsweapon";gun.name="Pulse laser";gun.key="1.";gun.mounted=.5f;gun.readiness=1;gun.selected=true;
            InventoryRow missile;missile.mdl="invsmissile";missile.name="Missile";missile.key="5.";missile.armed=.5f;missile.readiness=2;missile.count=10;
            state.inventory={gun,missile};
            auto directory=std::filesystem::temp_directory_path()/("allegiance-hud-styles-"+std::to_string(getpid()));std::filesystem::create_directories(directory);
            auto paint=[&](const char* name,bool transition=false){auto quads=styles.build(size,state,{},projection,{},false,false,true,{-1,-1});if(transition){SDL_Delay(800);quads=styles.build(size,state,{},projection,{},false,false,true,{-1,-1});}renderer.flushUploads();render({},{},quads,{},styles.preview());std::filesystem::copy_file(file,directory/(std::string(name)+".ppm"),std::filesystem::copy_options::overwrite_existing);};
            for(int style=0;style<6;++style){options.hudStyle=style;styles.configure(options);paint(("style-"+std::to_string(style)).c_str());
                auto* p=styles.preview();require(p && bool(p->next)==(style==3 || style==5),"authored target model views are missing or replace one another");
                require(!styles.click({float(size.x)-70,float(size.y)-100}),"classic undetectable inventory consumes flight mouse input");
                std::vector<double> timings;timings.reserve(512);auto resources=renderer.resources();
                for(int frame=0;frame<512;++frame){auto start=std::chrono::steady_clock::now();styles.build(size,state,{},projection,{},false,false,false);timings.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());}
                std::sort(timings.begin(),timings.end());require(renderer.resources().textures==resources.textures,"authored HUD creates textures during steady-state frames");
                printf("Authored HUD style %d: CPU build p50 %.2f us, p99 %.2f us; resident textures unchanged\n",style,timings[256],timings[506]);
            }
            options.hudStyle=0;styles.configure(options);state.commandView=true;state.camera=CameraMode::Command;state.coast=true;paint("command",true);
            state.docked=true;state.station="Garrison";state.stationSector="Wasp";state.stationHull=.5f;state.stationShield=.7f;state.stationColor={0,1,0,1};paint("station",true);
            state.docked=state.commandView=false;state.camera=CameraMode::Cockpit;state.pod=true;state.endurance=.4f;state.ripcord=true;state.ripcordLeft=3.4f;paint("pod-ripcord",true);
            state.pod=state.ripcord=false;state.ghost=state.dead=true;paint("death",true);
            printf("Original HUD pixels: six styles, both Glass target previews, faction/detection colors, objectives, command/station instruments, pod/ripcord/death and inventory art; captures in %s\n",directory.string().c_str());
        }
        {
            OriginalCursor cursors(assets);
            for(auto name:originalCursorNames)for(int scale:{1,2})require(cursors.set(name,scale),SDL_GetError());
            for(auto name:originalCursorNames)require(cursors.set(name,2,2),SDL_GetError());
#ifndef _WIN32
            const char* driver=SDL_GetCurrentVideoDriver();
            if(driver && (std::string_view(driver)=="wayland" || std::string_view(driver)=="cocoa")) {
                require(!cursors.set("default",1,2) && !cursors.set("default",1,1.5f),"cursor must preserve whole device pixels on incompatible/fractional densities");
            }
#endif
            const auto& c=cursors.artwork("capture");
            MenuView empty;empty.original=true;empty.overlay=false;
            hud.buildMenu(renderer.size(),empty,{});
            auto quads=hud.appendPointer(renderer.size(),{400,300},nullptr,&c);renderer.flushUploads();render({},{},quads);
            size_t probes=0;
            for(uint32_t y=0;y<c.image->height;++y)for(uint32_t x=0;x<c.image->width;++x) {
                auto i=(size_t(y)*c.image->width+x)*4;auto actual=pixel(file,400+int(c.bounds.x)+x,300+int(c.bounds.y)+y);
                for(int axis=0;axis<3;++axis)require(std::abs(actual[axis]-(c.image->pixels[i+3]?int(c.image->pixels[i+axis]):0))<=1,"original cursor fallback lost hotspot, color or alpha");++probes;
            }
            require(probes>50,"cursor probe lacks coverage");
            Settings trainingSettings=settings;trainingSettings.visual.centerHud=trainingSettings.visual.targetHud=trainingSettings.visual.messages=false;
            hud.configure(trainingSettings);GameState lesson;lesson.trainingMission=2;
            auto dot=assets.image("tm_hud_overlaybmp",true);
            uint32_t dotTexture=renderer.addTexture(dot,true);bool imageChecked=false;
            for(int i=SpeedTrainingOverlay;i<=CloakTrainingOverlay;++i) {
                lesson.trainingOverlay=i;quads=hud.build(renderer.size(),lesson,{},glm::mat4(1),{},false,false);
                auto callout=Allegiance::TrainingCalloutFor(TrainingOverlay(i),0,0,renderer.size().x,renderer.size().y);
                auto found=std::find_if(quads.begin(),quads.end(),[&](auto& q){return q.texture==dotTexture;});
                require(found!=quads.end() && found+1==quads.end(),"training highlight dot missing or drawn below its callout");
                auto redLines=std::count_if(quads.begin(),quads.end(),[](auto& q){return q.texture==hudLine && q.color==glm::vec4(1,0,0,.9f);});
                require(redLines==2,"original training callout needs two red segments");
                float x=callout.ax-std::floor(dot->width*.5f),y=renderer.size().y-callout.ay-std::floor(dot->height*.5f);
                require(std::abs(found->bounds.x-(x*2/renderer.size().x-1))<1e-6f && std::abs(found->bounds.y-(y*2/renderer.size().y-1))<1e-6f,"training dot moved from authored view anchor");
                renderer.flushUploads();render({},{},quads);
                if(i==FuelTrainingOverlay){std::filesystem::copy_file(file,"/tmp/allegiance-training-callout.ppm",std::filesystem::copy_options::overwrite_existing);imageChecked=true;}
            }
            require(imageChecked,"training artwork not rendered");hud.configure(settings);
            puts("Original cursor plane: all 15 icons at 1x/2x, exact fallback pixels/hotspot; all 23 authored training callouts rendered");
        }
        {
            // Expected faces transcribed from engine/button.cpp, independent
            // of the native selector. Exercise full and sparse authored strips.
            struct Strip {uint32_t mask;std::array<uint32_t,12> faces;};
            const Strip strips[]={
                {15,{1,2,4,8, 2,1,4,8, 2,1,4,8}},
                {255,{1,2,4,8, 16,32,64,128, 2,1,4,8}},
                {4095,{1,2,4,8, 16,32,64,128, 256,512,1024,2048}},
                {273,{1,1,1,0, 16,1,16,0, 256,1,256,0}},
                {2,{0,2,0,0, 2,0,0,0, 2,0,0,0}}
            };
            MenuView menu;menu.original=menu.overlay=true;auto layout=originalMenuLayout(renderer.size());
            OriginalControl button;button.kind=OriginalControl::Kind::Button;button.name="face";button.bounds={100,100,1,1};
            menu.artwork.push_back(button);menu.items.push_back({button.bounds,"",{}, {},true,false,"face"});
            unsigned cases=0;
            for(const auto& strip:strips){
                auto bitmap=std::make_shared<ImageData>();bitmap->width=1;bitmap->height=std::popcount(strip.mask);bitmap->mipmaps=false;bitmap->pixels.resize(bitmap->height*4,255);
                menu.artwork[0].image=bitmap;menu.artwork[0].faces=strip.mask;
                for(int checked=0;checked<4;++checked)for(int state=0;state<4;++state){
                    menu.artwork[0].checkedState=checked==3?0:checked;menu.items[0].selected=checked==3;menu.items[0].enabled=state!=3;menu.pressed=state==1?0:-1;
                    auto quads=hud.buildMenu(renderer.size(),menu,layout.pixel(state==1 || state==2?glm::vec2(100.5f):glm::vec2(-1)));
                    auto expected=strip.faces[(checked==3?1:checked)*4+state];
                    require(quads.size()==(expected?1u:0u),"sparse original button strip draws a missing face or drops its fallback");
                    if(expected){float row=std::popcount(strip.mask&(expected-1));require(quads[0].uv==glm::vec4(0,row/bitmap->height,1,(row+1)/bitmap->height),"original checked/pressed/hover/disabled button fallback selects the wrong bitmap row");}
                    ++cases;
                }
            }
            printf("Original button strips: %u normal/checked/second-checked states, sparse fallbacks and absent-face suppression passed\n",cases);
            Settings preferences;Frontend frontend(preferences);frontend.originalInterface(assets);frontend.open(Screen::Training);
            for(int lesson:{1,2}){
                auto select=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[&](const auto& item){return item.action.op==MenuOp::TrainingSelect && item.action.value==lesson;});require(select!=frontend.view().items.end(),"original training lesson control missing");frontend.activate(size_t(select-frontend.view().items.begin()));
                const auto& launch=authoredControl(frontend.view().artwork,"trainButtonPane");auto quads=hud.buildMenu(renderer.size(),frontend.view(),layout.pixel({-1,-1}));renderer.flushUploads();render({},{},quads);
                auto at=glm::ivec2(layout.pixel(glm::vec2(launch.bounds)));auto im=launch.image;auto height=im->height/4;size_t samples=0;
                for(unsigned y=1;y+1<height;y+=3)for(unsigned x=1;x+1<im->width;x+=3){auto offset=(size_t(y+height)*im->width+x)*4;if(im->pixels[offset+3]!=255)continue;auto actual=pixel(file,at.x+x,at.y+y);for(int c=0;c<3;++c)require(std::abs(actual[c]-int(im->pixels[offset+c]))<=1,"training Launch does not render its original checked/down face after selecting a lesson");++samples;}
                require(samples>30,"training Launch face probe misses the artwork");
            }
            puts("Original training pixels: Launch retains its checked artwork across lesson selection passed");
        }
        {
            Settings preferences;Frontend frontend(preferences);frontend.originalInterface(assets);frontend.setPlaying(true);
            GameState docked;docked.docked=true;docked.ship="Scout";docked.station="Home";docked.sector="Wasp";
            TeleportChoice ship{71,"Carrier","Aegis",false,false};ship.boarding=true;ship.ship="Bomber";ship.manned=2;ship.turrets=3;
            frontend.open(Screen::Hangar,&docked);frontend.teleports({ship,{12,"Home","Wasp",true,true}},docked);
            auto row=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](auto& c){return c.action.op==MenuOp::TeleportSelect && c.action.value==71;});
            require(row!=frontend.view().items.end(),"teleport pixels have no ship row");frontend.activate(size_t(row-frontend.view().items.begin()));
            auto menu=frontend.view();menu.slide->started=uiTime()-1;
            auto im=assets.image("teleportpanebmp",false);float top=400-im->height;
            auto layout=originalMenuLayout(renderer.size());
            auto renderMenu=[&]{auto quads=hud.buildMenu(renderer.size(),menu,{});renderer.flushUploads();render({},{},quads);};
            renderMenu();
            auto p=layout.pixel(glm::vec2(39+315,top+39+7)+menuSlideOffset(menu,renderer.size(),uiTime()));
            require(pixel(file,int(p.x),int(p.y))==std::array<int,3>{255,0,0},"original selected teleport row loses its red bar or screen-edge anchor");
            auto origin=layout.pixel(glm::vec2(36,top)+menuSlideOffset(menu,renderer.size(),uiTime()));
            size_t checked=0;
            for(int y=190;y<int(im->height)-1;++y)for(int x=1;x<11;++x){size_t offset=(size_t(y)*im->width+x)*4;
                if(!(im->pixels[offset]|im->pixels[offset+1]|im->pixels[offset+2]))continue;
                std::array<int,3> expected{im->pixels[offset],im->pixels[offset+1],im->pixels[offset+2]};
                auto actual=pixel(file,int(origin.x)+x,int(origin.y)+y);
                require(std::abs(actual[0]-expected[0])<=1 && std::abs(actual[1]-expected[1])<=1 && std::abs(actual[2]-expected[2])<=1,"teleport background is scaled, moved or recolored");++checked;
            }
            require(checked>20,"teleport pixel probe did not cover authored panel artwork");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-teleport.ppm",std::filesystem::copy_options::overwrite_existing);
            // Freeze at the geometric halfway displacement. Both opening and
            // closing use this same transform; completed time keeps the pixel
            // assertions independent of GPU/readback scheduling.
            menu.slide->off={-328,0};menu.slide->closing=true;renderMenu();
            p=layout.pixel(glm::vec2(39+315,top+39+7)+menuSlideOffset(menu,renderer.size(),uiTime()));
            require(pixel(file,int(p.x),int(p.y))==std::array<int,3>{255,0,0},"sliding teleport list clips its selected row at the old image position");
            auto close=std::find_if(menu.items.begin(),menu.items.end(),[](auto& c){return c.action.op==MenuOp::Resume;});require(close!=menu.items.end(),"teleport close hit missing");
            p=layout.pixel(glm::vec2(close->bounds)+glm::vec2(close->bounds.z,close->bounds.w)*.5f+menuSlideOffset(menu,renderer.size(),uiTime()));
            require(menuHit(menu,renderer.size(),1,p)==size_t(close-menu.items.begin()),"rendered halfway teleport close button cannot be hit");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-teleport-slide.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original teleport pixels: authored background, red selection, screen-edge placement and moving image/text/list clipping passed");
            frontend.error("Teleportation failed.");menu=frontend.view();menu.slide->started=uiTime()-1;menu.slide->off={-328,0};menu.slide->closing=true;
            renderMenu();
            auto dialog=assets.image("dialogbkgndbmp",true);auto at=glm::floor((glm::vec2(800,600)-glm::vec2(dialog->width,dialog->height))*.5f);
            auto dialogOrigin=layout.pixel(at);size_t covered=0;
            for(int y=3;y<int(dialog->height)-3;++y)for(int x=3;x<20;++x){auto offset=(size_t(y)*dialog->width+x)*4;if(dialog->pixels[offset+3]!=255)continue;
                auto actual=pixel(file,int(dialogOrigin.x)+x,int(dialogOrigin.y)+y);
                for(int c=0;c<3;++c)require(std::abs(actual[c]-int(dialog->pixels[offset+c]))<=1,"message-box artwork is recolored, scaled or translated with the console");++covered;}
            require(covered>20,"modal pixel probe missed the original background");
            auto ok=std::find_if(menu.items.begin(),menu.items.end(),[](auto& item){return item.action.op==MenuOp::MessageDismiss;});require(ok!=menu.items.end(),"modal OK pixel/hit missing");
            p=layout.pixel(glm::vec2(ok->bounds)+glm::vec2(ok->bounds.z,ok->bounds.w)*.5f);
            require(menuHit(menu,renderer.size(),1,p)==size_t(ok-menu.items.begin()),"modal OK is displaced by the underlying panel animation");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-messagebox.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original message box pixels: unchanged centered artwork and input geometry above animated teleport pane passed");
            frontend.open(Screen::Hangar,&docked);
            for(auto transfer:{Allegiance::CrewTransfer::Board,Allegiance::CrewTransfer::Disembark,Allegiance::CrewTransfer::Teleport}){
                frontend.transferring(transfer,transfer==Allegiance::CrewTransfer::Teleport?"Aegis Garrison":"Carrier pilot",71);menu=frontend.view();renderMenu();
                const auto& text=authoredControl(menu.artwork,"messageText");
                require(text.font=="largeFont" && text.textMode==OriginalControl::TextMode::Image && text.bounds.x==at.x+50 && text.bounds.y==at.y+50,"transfer modal changes original bitmap font or text placement");
                require(std::none_of(menu.items.begin(),menu.items.end(),[](const auto& item){return item.enabled && !item.blocked;}),"pending transfer leaves an active OK or background action");
                for(const auto& item:menu.items){auto p=layout.pixel(glm::vec2(item.bounds)+glm::vec2(item.bounds.z,item.bounds.w)*.5f);require(!menuHit(menu,renderer.size(),1,p),"pending transfer lets a pointer hit its background controls");}
                for(int y=3;y<int(dialog->height)-3;y+=7)for(int x=3;x<20;x+=3){auto offset=(size_t(y)*dialog->width+x)*4;if(dialog->pixels[offset+3]!=255)continue;auto actual=pixel(file,int(dialogOrigin.x)+x,int(dialogOrigin.y)+y);for(int c=0;c<3;++c)require(std::abs(actual[c]-int(dialog->pixels[offset+c]))<=1,"transfer modal changes original background artwork or integer placement");}
                auto name=transfer==Allegiance::CrewTransfer::Board?"boarding":transfer==Allegiance::CrewTransfer::Disembark?"disembarking":"teleporting";
                std::filesystem::copy_file(file,file.parent_path()/(std::string("allegiance-native-")+name+".ppm"),std::filesystem::copy_options::overwrite_existing);
                frontend.transferFinished(71);
            }
            puts("Original transfer modal pixels: boarding/disembark/teleport text, bitmap font, centered 1x artwork and disabled background input passed");
        }
        {
            Settings preferences;Frontend frontend(preferences);frontend.originalInterface(assets);frontend.setPlaying(true);
            GameState docked;docked.economy=true;docked.money=4321;docked.docked=true;docked.ship="Scout";docked.station="Home";docked.sector="Wasp";
            std::vector<ResearchChoice> rows;
            for(int i=0;i<8;++i){ResearchChoice row;row.id=i;row.type=OT_droneType;row.group=0;row.price=100;row.name=i?"Miner":"Scout";row.model="acs38";row.available=row.canBuy=row.predecessorReady=true;row.limit=100;row.description="Send scouts through alephs to discover sectors, enemy ships and mineral deposits.";rows.push_back(row);}
            frontend.researchService([&]{return rows;});frontend.open(Screen::Research,&docked);
            auto item=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](auto& item){return item.action.op==MenuOp::ResearchSelect && item.action.value==0;});frontend.pointerHover(size_t(item-frontend.view().items.begin()));
            auto menu=frontend.view();menu.slide->started=uiTime()-1;auto layout=originalMenuLayout(renderer.size());
            auto draw=[&]{auto quads=hud.buildMenu(renderer.size(),menu,{});renderer.flushUploads();render({},{},quads);};draw();
            auto im=assets.image("conpurchbmp",false);auto origin=layout.pixel(glm::vec2(310,565-im->height)+menuSlideOffset(menu,renderer.size(),uiTime()));
            size_t checked=0;for(int y=4;y<300;++y)for(int x=1;x<12;++x){auto offset=(size_t(y)*im->width+x)*4;if(!(im->pixels[offset]|im->pixels[offset+1]|im->pixels[offset+2]))continue;auto actual=pixel(file,int(origin.x)+x,int(origin.y)+y);for(int c=0;c<3;++c)require(std::abs(actual[c]-int(im->pixels[offset+c]))<=1,"investment background is scaled, moved or recolored");++checked;}require(checked>100,"investment pixel probe missed artwork");
            auto card=assets.image("iacs38bmp",true);auto unselected=std::find_if(menu.artwork.begin(),menu.artwork.end(),[&](const auto& c){return c.image==card && c.bounds.y>150;});
            require(unselected!=menu.artwork.end(),"render fixture fell back to a blank research card");
            auto cardOrigin=layout.pixel(glm::vec2(unselected->bounds)+menuSlideOffset(menu,renderer.size(),uiTime()));size_t cardPixels=0;
            for(int y=20;y<std::min(38,int(card->height));++y)for(int x=4;x<std::min(170,int(card->width));x+=3){auto offset=(size_t(y)*card->width+x)*4;if(card->pixels[offset+3]!=255)continue;auto actual=pixel(file,int(cardOrigin.x)+x,int(cardOrigin.y)+y);for(int c=0;c<3;++c)require(std::abs(actual[c]-int(card->pixels[offset+c]))<=1,"research model card differs from original authored pixels");++cardPixels;}
            require(cardPixels>100,"research card art probe did not exercise its model picture");
            auto point=layout.pixel(glm::vec2(310+227+8+40,127+9+2+35)+menuSlideOffset(menu,renderer.size(),uiTime()));auto base=pixel(file,int(point.x),int(point.y));
            rows[0].progress=50;rows[0].bought=75;rows[0].invested=75;++docked.economyRevision;frontend.sync(docked);menu=frontend.view();menu.slide->started=uiTime()-1;draw();auto green=pixel(file,int(point.x),int(point.y));
            require(green[1]>base[1]+20 && green[0]<=base[0] && green[2]<=base[2],"completed investment shading has wrong color/extent/blend");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-investments.ppm",std::filesystem::copy_options::overwrite_existing);
            auto close=std::find_if(menu.items.begin(),menu.items.end(),[](auto& item){return item.action.op==MenuOp::Resume;});menu.slide->off={250,0};menu.slide->closing=true;draw();
            auto b=close->bounds;auto p=layout.pixel(glm::vec2(b)+glm::vec2(b.z,b.w)*.5f+menuSlideOffset(menu,renderer.size(),uiTime()));
            if(p.x<renderer.size().x)require(menuHit(menu,renderer.size(),1,p)==size_t(close-menu.items.begin()),"sliding investment image/hits disagree");
            puts("Original investment pixels: authored background, native fonts, card art, hover info, green/paid shading and right-side slide passed");
        }
        {
            Hud mapHud(renderer,assets);mapHud.configure(settings);GameState state;state.playerSide=0;state.sideColors[0]={0,1,0,1};
            state.sectors={{7,"Wasp",{0,0}},{9,"Unknown",{1,1}}};state.sectors[0].current=true;state.sectors[0].owner=0;state.sectors[0].population[0]=7;state.sectors[1].known=false;
            SectorInfo information;information.id=7;information.helium=123;mapHud.sectorInfoService([&](int id)->const SectorInfo&{require(id==7,"hidden sector selected for map information");return information;});
            auto size=renderer.size();auto draw=[&]{auto quads=mapHud.build(size,state,{},projection,{},false,false,true,{-1,-1});renderer.flushUploads();render({},{},quads);};draw();
            auto panel=assets.image("CortUI/classic/minimappanelexpanded.png",true);glm::ivec2 origin{49,size.y-177};
            for(int y=45;y<110;++y)for(int x=3;x<8;++x){auto expected=(size_t(y)*panel->width+x)*4;auto actual=pixel(file,origin.x+x,origin.y+y);for(int c=0;c<3;++c)require(std::abs(actual[c]-int(panel->pixels[expected+c]))<=1,"sector panel artwork is scaled/moved/recolored");}
            glm::vec2 center=glm::vec2(origin)+glm::vec2(9+60,25+47);
            auto left=mapHud.mapPointerButton(center,SDL_BUTTON_LEFT,true),right=mapHud.mapPointerButton(center,SDL_BUTTON_RIGHT,true),highlight=mapHud.mapPointerButton(center,SDL_BUTTON_RIGHT,true,true);
            require(left.action && left.action->command==HudCommand::Sector && left.action->value==7 && right.action && right.action->command==HudCommand::SectorChat && highlight.action && highlight.action->command==HudCommand::SectorHighlight,"map click verbs differ from original left/right/control-right routes");
            require(!mapHud.mapPointerButton({800,100},SDL_BUTTON_LEFT,true).handled,"map consumes unrelated flight clicks");
            auto expand=glm::vec2(origin)+glm::vec2(183,9);mapHud.mapPointerButton(expand,SDL_BUTTON_LEFT,true);mapHud.mapPointerButton(expand,SDL_BUTTON_LEFT,false);draw();
            auto collapsed=assets.image("CortUI/classic/minimappanel.png",true);auto actual=pixel(file,origin.x+3,origin.y+60);auto expected=(size_t(60)*collapsed->width+3)*4;
            for(int c=0;c<3;++c)require(std::abs(actual[c]-int(collapsed->pixels[expected+c]))<=1,"expand button does not select original collapsed artwork");
            mapHud.binding(Binding::SectorMap);mapHud.build(size,state,{},projection,{},false,false,true);
            require(!mapHud.mapPointerButton(center,SDL_BUTTON_LEFT,true).handled,"closing map remains clickable");
            mapHud.build(size,state,{},projection,{},false,false,true,{-1,-1},nullptr,nullptr,{},ofInvestment);
            // WinTrek restarts an entrance slide at the authored off point,
            // including reversal during a closing slide. Click the final map.
            SDL_Delay(510);
            mapHud.build(size,state,{},projection,{},false,false,true,{-1,-1},nullptr,nullptr,{},ofInvestment);
            require(mapHud.mapPointerButton(center,SDL_BUTTON_LEFT,true).action.has_value(),"F5 must force map visibility independently of F7");
            state.sectors[0].position={-5,-5};state.sectors[1].position={5,5};state.sectors[1].known=true;
            mapHud.build(size,state,{},projection,{},false,false,true,{-1,-1},nullptr,nullptr,{},ofInvestment);
            auto blank=glm::vec2(origin)+glm::vec2(9+60,25+47);auto drag=mapHud.mapPointerButton(blank,SDL_BUTTON_LEFT,true);
            require(drag.handled && drag.capture && mapHud.mapPointerMove(blank+glm::vec2(-80,60)),"large map does not capture original empty-map drag");
            require(mapHud.mapPointerButton(blank,SDL_BUTTON_LEFT,false).handled && !mapHud.mapPointerMove(blank),"map drag capture does not release");
            MenuView menu;menu.original=true;menu.canvas={800,600};mapHud.buildMenu(size,menu,{});
            require(!mapHud.mapPointerButton(blank,SDL_BUTTON_LEFT,true).handled,"hidden map retains stale input behind menus");
            puts("Sector map pixels/input: original expanded/collapsed panels, discovery, mouse verbs, F5 visibility, drag capture and inactive exit/menu hits passed");
        }
        {
            GameState state;state.sectors.push_back({7,"Build site",{0,0}});
            auto ordinary=hud.build(renderer.size(),state,{},projection,{},false,false);
            std::vector<HudQuad> base(ordinary.begin(),ordinary.end());std::array<int,1> sites{7};
            auto marked=hud.build(renderer.size(),state,{},projection,{},false,false,false,{-1,-1},nullptr,nullptr,sites);
            require(marked.size()==base.size()+1,"construction hints change unrelated HUD geometry");
            auto source=assets.image("sectorripcordbmp",true);auto texture=renderer.addTexture(source,true);
            auto marker=std::find_if(marked.begin(),marked.end(),[&](const auto& quad){return quad.texture==texture;});
            require(marker!=marked.end() && marker->uv==glm::vec4(0,0,1,1) && marker->color==glm::vec4(1),"construction hint is not the original keyed sector marker");
            renderer.flushUploads();render({},{},marked);
            auto clear=hud.build(renderer.size(),state,{},projection,{},false,false);
            require(clear.size()==base.size() && std::none_of(clear.begin(),clear.end(),[&](const auto& quad){return quad.texture==texture;}),"construction hint persists after leaving the card");
            puts("Investment sector hints: original keyed marker, ordered HUD overlay and clearing passed");
        }

        {
            auto pattern=std::make_shared<ImageData>();pattern->width=pattern->height=2;pattern->mipmaps=false;pattern->pixels={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
            auto linear=renderer.addTexture(pattern);auto resources=renderer.resources();auto nearest=renderer.addTexture(pattern,true);
            require(linear!=nearest && renderer.addTexture(pattern,true)==nearest && renderer.addTexture(pattern)==linear,"sampling variants do not retain independent descriptors");
            require(renderer.resources().textures==resources.textures,"nearest UI descriptor duplicates GPU image/upload");
            MenuView menu;menu.original=true;menu.canvas={32,24};OriginalControl c;c.kind=OriginalControl::Kind::Image;c.image=pattern;c.bounds={4,4,2,2};menu.artwork.push_back(c);
            std::array<std::array<int,3>,4> colors{{{255,0,0},{0,255,0},{0,0,255},{255,255,255}}};
            for(int factor:{1,2,3,8}){
                menu.classicScale=factor;auto overlay=hud.buildMenu(renderer.size(),menu,{});renderer.flushUploads();render({},{},overlay);
                auto layout=originalMenuLayout(renderer.size(),factor,menu.canvas);require(layout.scale==factor,"small pixel probe cannot reach requested scale");
                auto p=glm::ivec2(layout.pixel({4,4}));
                for(int y=0;y<2*factor;++y)for(int x=0;x<2*factor;++x)
                    require(pixel(file,p.x+x,p.y+y)==colors[size_t(y/factor)*2+x/factor],"classic UI upscale blends or shifts authored pixels");
                require(pixel(file,p.x-1,p.y)==std::array<int,3>{0,0,0} && pixel(file,p.x+2*factor,p.y)==std::array<int,3>{0,0,0},"classic image covers pixels outside its integer bounds");
            }
            puts("Classic nearest sampling: exact 1x/2x/3x/8x pixels with one resident GPU image passed");
        }
        {
            MenuView menu;menu.original=true;auto layout=originalMenuLayout(renderer.size());
            NativeTest::AuthoredAssets fixture(assets.root());
            {std::ofstream mdl(fixture.root/"native_hover_fixture.mdl");mdl<<R"(firstSite=HoverSite(0);secondSite=HoverSite(0);
firstHelp=SwitchString(HoverSiteGetID(firstSite),"",[(7,"FIRST")]);secondHelp=SwitchString(HoverSiteGetID(secondSite),"",[(7,"SECOND")]);
region0=ListPane(Point(80,20));region1=ListPane(Point(80,20));
arbitraryFirstName=StringPane(firstHelp,Color(1,1,1),Point(50,12),JustifyLeft,medVerdana);
arbitrarySecondName=SwitchPane(HoverSiteGetID(secondSite),[(StringPane("SECOND",Color(.2,.4,.6),Point(50,12),JustifyRight,smallFont),7),(StringPane("",Color(1,1,1),Point(50,12),JustifyLeft,medVerdana),0)]);
screen=ScreenPane(ImportImage("hangarbmp",false),[(HoverPane(firstSite,7,region0),Point(100,30)),(HoverPane(secondSite,7,region1),Point(200,30)),(arbitraryFirstName,Point(100,100)),(arbitrarySecondName,Point(200,100))]);)";}
            Assets changed(fixture.root);OriginalUi authored(changed);menu.artwork=authored.layout("native_hover_fixture");menu.artwork.erase(menu.artwork.begin());
            for(int i=0;i<2;++i){auto quads=hud.buildMenu(renderer.size(),menu,layout.pixel({110+i*100,40}));require(quads.size()>1,"authored hover text remains blank");
                for(size_t q=1;q<quads.size();++q){auto low=(glm::vec2(quads[q].bounds)+glm::vec2(1))*glm::vec2(renderer.size())*.5f,high=(glm::vec2(quads[q].bounds.z,quads[q].bounds.w)+glm::vec2(1))*glm::vec2(renderer.size())*.5f;
                    auto a=layout.pixel({100+i*100,100}),b=layout.pixel({150+i*100,112});require(low.x>=a.x-.01f && low.y>=a.y-.01f && high.x<=b.x+.01f && high.y<=b.y+.01f,"hover text crosses its MDL site or authored clip rectangle");}
                renderer.flushUploads();render({},{},quads);bool glyph=false;auto at=glm::ivec2(layout.pixel({100+i*100,100}));
                for(int y=0;y<12;++y)for(int x=0;x<50;++x){auto color=pixel(file,at.x+x,at.y+y);auto expected=i?std::array<int,3>{51,102,153}:std::array<int,3>{255,255,255};if(std::abs(color[0]-expected[0])<=1 && std::abs(color[1]-expected[1])<=1 && std::abs(color[2]-expected[2])<=1)glyph=true;}
                require(glyph,"Hover GPU pixels lose the selected authored branch color/font");}
            require(hud.buildMenu(renderer.size(),menu,layout.pixel({10,10})).size()==1,"leaving hover retains stale text");
            auto resources=renderer.resources();for(int i=0;i<64;++i)hud.buildMenu(renderer.size(),menu,layout.pixel({110+i%2*100,40}));require(renderer.resources().textures==resources.textures,"Hover branch selection creates per-frame GPU resources");
            MenuView layered=menu;std::vector<OriginalControl> dialog;
            std::erase_if(layered.artwork,[&](const auto& c){bool child=c.name=="region1" || c.name=="arbitrarySecondName" || c.helpSite=="native_hover_fixture::secondSite";if(child)dialog.push_back(c);return child;});
            layered.items.push_back({{100,30,80,20},"",{}, {},true,false,"region0"});layered.blockInput();
            layered.artwork.insert(layered.artwork.end(),dialog.begin(),dialog.end());layered.items.push_back({{200,30,80,20},"",{}, {},true,false,"region1"});
            require(hud.buildMenu(renderer.size(),layered,layout.pixel({110,40})).size()==1,"Layered modal keeps the blocked parent's hover active");
            auto childQuads=hud.buildMenu(renderer.size(),layered,layout.pixel({210,40}));require(childQuads.size()>1,"Blocked parent suppresses the active child dialog's hover");
            render({},{},childQuads);bool childGlyph=false;auto childAt=glm::ivec2(layout.pixel({200,100}));
            for(int y=0;y<12;++y)for(int x=0;x<50;++x){auto color=pixel(file,childAt.x+x,childAt.y+y);if(std::abs(color[0]-51)<=1 && std::abs(color[1]-102)<=1 && std::abs(color[2]-153)<=1)childGlyph=true;}
            require(childGlyph,"Active dialog loses its authored hover pixels above a blocked parent");
            menu.items.push_back({{100,30,80,20},"",{}, {},false,false,"region0"});
            require(hud.buildMenu(renderer.size(),menu,layout.pixel({110,40})).size()>1,"disabled but visible hover wrapper is inactive");
            std::erase_if(menu.artwork,[](const auto& c){return c.name=="region0";});
            require(hud.buildMenu(renderer.size(),menu,layout.pixel({110,40})).size()==1,"hidden child retains its hover wrapper");
            menu.blockInput();
            require(hud.buildMenu(renderer.size(),menu,layout.pixel({210,40})).size()==1,"modal-covered hover regions remain active");
            puts("Authored hover drawing: two independent sites, arbitrary sink names, exact clips, visible-disabled/hidden targets, leave clearing, blocked parents and active child dialogs passed");
        }
        {
            MenuView menu;menu.original=true;menu.shipPreview=ModelHandle{id,1};OriginalControl viewport;viewport.kind=OriginalControl::Kind::Slot;viewport.name="loadoutGeoPane";viewport.bounds={73,83,151,99};menu.artwork.push_back(viewport);
            auto quads=hud.buildMenu(renderer.size(),menu,{});auto p=hud.preview();auto layout=originalMenuLayout(renderer.size());
            require(p && p->bounds==glm::ivec4(glm::ivec2(layout.pixel({73,83})),151,99),"Loadout preview GPU viewport retains copied asset geometry");
            render({},{},quads,{},p);auto at=glm::ivec2(p->bounds);int visible=0;
            for(int y=0;y<99;y+=3)for(int x=0;x<151;x+=3){auto color=pixel(file,at.x+x,at.y+y);if(color[0]+color[1]+color[2]>30)++visible;}
            require(visible>20 && pixel(file,at.x-1,at.y+49)==std::array<int,3>{0,0,0} && pixel(file,at.x+151,at.y+49)==std::array<int,3>{0,0,0},"Moved loadout model is missing or escapes its authored viewport");
            menu.artwork.front().clip=glm::vec4(73,83,76,99);quads=hud.buildMenu(renderer.size(),menu,{});p=hud.preview();require(p && p->clip && p->bounds.z==151,"Preview clipping distorts the original viewport/aspect");
            render({},{},quads,{},p);visible=0;
            for(int y=0;y<99;y+=3)for(int x=0;x<151;x+=3){auto color=pixel(file,at.x+x,at.y+y);if(x<76 && color[0]+color[1]+color[2]>30)++visible;else if(x>=76)require(color==std::array<int,3>{0,0,0},"Preview model escapes the inherited pane clip");}require(visible>10,"Clipped preview lost its visible portion");
            puts("Loadout preview pixels: authored position/extent/aspect, ordered model insertion and exact viewport boundaries passed");
        }
        {
            MenuView menu;menu.original=true;OriginalControl text;text.kind=OriginalControl::Kind::Text;text.font="medVerdana";text.text="Original bitmap font";text.bounds={100,100,400,20};menu.artwork.push_back(text);
            auto result=hud.buildMenu(renderer.size(),menu,{});std::vector<HudQuad> plain(result.begin(),result.end());
            require(plain.size()>10,"original menu text emits no glyphs without clipping");
            menu.artwork.front().clip=glm::vec4(0,0,800,600);result=hud.buildMenu(renderer.size(),menu,{});
            require(result.size()==plain.size(),"menu clipping changes visible glyph count");
            for(size_t i=0;i<plain.size();++i)for(int axis=0;axis<4;++axis)require(std::abs(plain[i].bounds[axis]-result[i].bounds[axis])<.00001f && std::abs(plain[i].uv[axis]-result[i].uv[axis])<.00001f,"unclipped and fully visible clipped menu text differ");
            renderer.flushUploads();render({},{},plain);
            std::ifstream in(file,std::ios::binary);std::string magic;int w,h,maximum;in>>magic>>w>>h>>maximum;in.get();std::vector<unsigned char> pixels(size_t(w)*h*3);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
            require(std::count_if(pixels.begin(),pixels.end(),[](unsigned char c){return c>100;})>100,"original bitmap glyphs emit quads but are invisible on the GPU");
            auto metrics=assets.font("medVerdana");auto advance=metrics->glyphs['W'].z;
            auto& raw=menu.artwork.front();raw.text="WW";raw.singleLine=true;raw.bounds.z=advance*1.5f;raw.clip=glm::vec4(100,100,raw.bounds.z,20);raw.clipCharacters=false;
            result=hud.buildMenu(renderer.size(),menu,{});std::vector<HudQuad> cut(result.begin(),result.end());raw.clipCharacters=true;result=hud.buildMenu(renderer.size(),menu,{});
            require(cut.size()==result.size()+1,"raw original list painter drops partially clipped glyphs");
            auto fraction=(cut.back().uv.z-cut.back().uv.x)*metrics->coverage->width;
            require(std::abs(fraction-advance*.5f)<.001f,"partial list glyph has incorrect clipped texture coordinates");
            raw.clip.reset();raw.text="Authored system font";raw.font="Verdana";raw.fontHeight=18;raw.fontBold=true;raw.bounds.z=400;
            result=hud.buildMenu(renderer.size(),menu,{});std::vector<HudQuad> system(result.begin(),result.end());renderer.flushUploads();render({},{},system);
            auto cached=renderer.resources();for(int i=0;i<64;++i)hud.buildMenu(renderer.size(),menu,{});require(renderer.resources().textures==cached.textures,"system-font menus create per-frame GPU resources");
            auto paneFont=assets.font("medVerdana");MenuView textMenu;textMenu.original=true;OriginalControl control;control.kind=OriginalControl::Kind::Text;control.textMode=OriginalControl::TextMode::Pane;control.singleLine=true;control.font="medVerdana";control.text="MMMMiiii";control.bounds={100,100,4*paneFont->glyphs['i'].z+1,20};control.justify=3;textMenu.artwork.push_back(control);
            auto suffix=hud.buildMenu(renderer.size(),textMenu,{});require(suffix.size()==5,"StringPane clip-right must retain four trailing glyphs");
            auto layout=originalMenuLayout(renderer.size());auto point=layout.pixel({100,100});auto first=suffix[1];auto actual=(glm::vec2(first.bounds)+glm::vec2(1))*glm::vec2(renderer.size())*.5f;
            require(std::abs(actual.x-point.x)<.01f && std::abs(first.uv.x-paneFont->glyphs['i'].x/paneFont->coverage->width)<.0001f,"StringPane clip-right draws the prefix or right-aligns its suffix");
            textMenu.artwork.front().bounds.z=0;require(hud.buildMenu(renderer.size(),textMenu,{}).size()==1,"zero-width StringPane draws unbounded text");
            control.textMode=OriginalControl::TextMode::Image;control.singleLine=false;control.justify=0;control.text="MMMM MMMM MMMM MMMM";control.bounds.z=7*paneFont->glyphs['M'].z;control.bounds.w=paneFont->height*4;textMenu.artwork.front()=control;
            auto wrapped=hud.buildMenu(renderer.size(),textMenu,{});std::array<bool,4> lines{};for(size_t i=1;i<wrapped.size();++i){auto top=(wrapped[i].bounds.y+1)*renderer.size().y*.5f;int row=int(std::round((top-point.y)/paneFont->height));require(row>=0 && row<4,"StringImage emits glyphs outside its measured wrapped height");lines[row]=true;}
            require(std::all_of(lines.begin(),lines.end(),[](bool line){return line;}),"StringImage wrapping uses character-only height or omits its final word");
            puts("Original bitmap text pixels and clipped/unclipped glyph parity passed");
            // Overlay menus preserve the scene and the HUD's existing batch.
            // They must not inject the opaque full-screen menu clear.
            GameState state;auto base=hud.build(renderer.size(),state,{},projection,{},false,false);
            std::vector<HudQuad> baseCopy(base.begin(),base.end());menu.overlay=true;
            auto appended=hud.buildMenu(renderer.size(),menu,{},true);
            require(appended.size()>baseCopy.size(),"original popup does not append to game HUD");
            for(size_t i=0;i<baseCopy.size();++i)require(appended[i].bounds==baseCopy[i].bounds && appended[i].uv==baseCopy[i].uv && appended[i].color==baseCopy[i].color && appended[i].texture==baseCopy[i].texture,"appending popup mutates existing HUD");
            for(size_t i=baseCopy.size();i<appended.size();++i)require(appended[i].bounds!=glm::vec4(-1,-1,1,1),"original overlay clears the entire flight view");
            auto transparent=hud.buildMenu(renderer.size(),menu,{});std::vector<HudQuad> transparentCopy(transparent.begin(),transparent.end());
            require(transparentCopy.size()+1==plain.size(),"overlay retains full-screen background quad");
            auto overlayPixel=render({&backdrop,1},{},transparentCopy);
            require(overlayPixel==background,"original overlay obscures scene outside its controls");
            state.commandView=true;state.camera=CameraMode::Command;hud.build(renderer.size(),state,{},projection,{},false,false);
            SDL_Delay(510);hud.build(renderer.size(),state,{},projection,{},false,false);
            std::optional<HudAction> commandHit;glm::vec2 commandPixel{};
            for(int y=0;y<renderer.size().y && !commandHit;y+=4)for(int x=0;x<renderer.size().x && !commandHit;x+=4){auto hit=hud.click({x,y});if(hit && hit->command==HudCommand::CommandOrder){commandHit=hit;commandPixel={x,y};}}
            require(bool(commandHit),"command HUD hit fixture missing");
            menu.canvas={32,24};menu.classicScale=3;hud.buildMenu(renderer.size(),menu,{},true);
            auto retained=hud.click(commandPixel);require(retained && retained->command==commandHit->command && retained->value==commandHit->value,"overlay destroys underlying command hits or changes their coordinate scale");
            puts("Original popup overlay preserves flight scene and HUD batch");

        }
        {
            MenuView menu;menu.original=true;auto font=assets.font("largeFont");auto layout=originalMenuLayout(renderer.size());
            OriginalControl edit;edit.kind=OriginalControl::Kind::Text;edit.textMode=OriginalControl::TextMode::Edit;edit.singleLine=true;edit.font="largeFont";edit.text="MMMMiiii";
            float tail=4*font->glyphs['i'].z,cursor=font->glyphs['o'].z;edit.bounds={100,100,tail+cursor,26};edit.clip=edit.bounds;edit.editFocused=true;edit.editLastTyped=uiTime();menu.artwork.push_back(edit);
            auto result=hud.buildMenu(renderer.size(),menu,{});require(result.size()==6,"focused EditPane must retain four trailing glyphs and its filled cursor");
            auto low=[&](const HudQuad& q){return (glm::vec2(q.bounds)+glm::vec2(1))*glm::vec2(renderer.size())*.5f;};
            auto high=[&](const HudQuad& q){return (glm::vec2(q.bounds.z,q.bounds.w)+glm::vec2(1))*glm::vec2(renderer.size())*.5f;};
            float free=edit.bounds.w-font->height;auto glyph=layout.pixel({100,100+(free>1?std::floor(free/2):0)}),cursorLow=layout.pixel({101+tail,101}),cursorHigh=layout.pixel({100+tail+cursor,124});
            require(glm::length(low(result[1])-glyph)<.01f && std::abs(result[1].uv.x-font->glyphs['i'].x/font->coverage->width)<.0001f,"EditPane suffix or vertical centering differs from its original font");
            require(glm::length(low(result.back())-cursorLow)<.01f && glm::length(high(result.back())-cursorHigh)<.01f,"EditPane filled cursor differs from original pane bounds or o width");
            renderer.flushUploads();render({},{},result);
            for(int y=int(cursorLow.y);y<int(cursorHigh.y);++y)for(int x=int(cursorLow.x);x<int(cursorHigh.x);++x)require(pixel(file,x,y)==std::array<int,3>{255,255,255},"EditPane cursor rectangle is invisible or has incorrect GPU pixels");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-edit-focused.ppm",std::filesystem::copy_options::overwrite_existing);
            auto& c=menu.artwork.front();c.editFocused=false;result=hud.buildMenu(renderer.size(),menu,{});
            require(result.size()>1 && std::abs(result[1].uv.x-font->glyphs['M'].x/font->coverage->width)<.0001f,"unfocused EditPane must draw its prefix without an artificial cursor glyph");
            c.editFocused=true;c.editReadOnly=true;result=hud.buildMenu(renderer.size(),menu,{});require(result.size()>1 && std::abs(result[1].uv.x-font->glyphs['M'].x/font->coverage->width)<.0001f,"readonly EditPane incorrectly retains its focused suffix");
            c.editReadOnly=false;c.text.clear();c.editLastTyped=uiTime();result=hud.buildMenu(renderer.size(),menu,{});require(result.size()==2 && glm::length(low(result.back())-layout.pixel({101,101}))<.01f,"empty focused EditPane loses its end cursor");
            c.editLastTyped=uiTime()-.6;require(hud.buildMenu(renderer.size(),menu,{}).size()==1,"EditPane blink-off retains a cursor or underscore");
            c.text="Mi";c.bounds.z=100;c.clip=c.bounds;c.editPassword=true;c.editLastTyped=uiTime();result=hud.buildMenu(renderer.size(),menu,{});
            require(result.size()==4 && result[1].uv==result[2].uv && std::abs(result[1].uv.x-font->glyphs['*'].x/font->coverage->width)<.0001f,"password EditPane draws raw character glyphs");
            auto cached=renderer.resources();for(int i=0;i<64;++i)hud.buildMenu(renderer.size(),menu,{});require(renderer.resources().textures==cached.textures,"EditPane blinking or password drawing creates per-frame GPU resources");
            puts("Original EditPane GPU pixels: suffix/prefix/read-only/password clipping, authored font centering, exact filled and empty cursors, blink and retained textures passed");
        }
        {
            OriginalUi original(assets);MenuView menu;menu.original=true;menu.artwork=original.layout("gamescreen","createDialog");
            auto face=std::find_if(menu.artwork.begin(),menu.artwork.end(),[](const auto& c){return c.name=="createEditPane";});require(face!=menu.artwork.end(),"original Create Game edit missing from clipping fixture");
            auto edit=widgetText(*face,std::string(100,'M'));edit.textMode=OriginalControl::TextMode::Edit;edit.editFocused=true;edit.editLastTyped=uiTime();menu.artwork.push_back(edit);
            for(auto& c:menu.artwork)c.translate({51,40});auto layout=originalMenuLayout(renderer.size());auto parent=menu.artwork.front().bounds;
            auto result=hud.buildMenu(renderer.size(),menu,{});renderer.flushUploads();render({},{},result);auto right=layout.pixel({parent.x+parent.z,0}).x;
            auto start=layout.pixel({edit.bounds.x+51,edit.bounds.y+40});auto bottom=start.y+edit.bounds.w;
            for(int y=int(start.y);y<int(bottom);++y)for(int x=int(right);x<int(right+40);++x)require(pixel(file,x,y)==std::array<int,3>{0,0,0},"oversized authored edit escapes its translated parent surface in Vulkan pixels");
            size_t white=0;for(int y=int(start.y);y<int(bottom);++y)for(int x=int(start.x);x<int(right);++x){auto p=pixel(file,x,y);white+=p[0]>250 && p[1]>250 && p[2]>250;}require(white>50,"parent clipping removes the visible portion of its authored edit");
            std::filesystem::copy_file(file,"/tmp/allegiance-native-parent-clipping.ppm",std::filesystem::copy_options::overwrite_existing);
            auto resources=renderer.resources();for(int i=0;i<64;++i)hud.buildMenu(renderer.size(),menu,{});require(renderer.resources().textures==resources.textures,"inherited clipping creates per-frame textures");
            puts("Original parent clipping GPU pixels: translated MDL Create Game host, visible edit glyphs, blank pixels beyond parent and retained textures passed");
        }
        {
            auto font=assets.font("largeFont");auto layout=originalMenuLayout(renderer.size());MenuView menu;menu.original=true;
            OriginalControl c;c.kind=OriginalControl::Kind::Text;c.font="largeFont";c.textMode=OriginalControl::TextMode::Pane;c.singleLine=true;c.bounds={100,100,300,26};c.clip=c.bounds;c.color={0,1,0,1};c.justify=2;c.text="\x81 FF0000FFM\x82 W";menu.artwork.push_back(c);
            auto result=hud.buildMenu(renderer.size(),menu,{});require(result.size()==3 && result[1].color==glm::vec4(1,0,0,1) && result[2].color==glm::vec4(0,1,0,1),"original inline controls draw bytes, lose their separator or fail to restore the supplied color");
            float used=font->glyphs['M'].z+font->glyphs['W'].z;auto start=layout.pixel({400-used,100});auto actual=(glm::vec2(result[1].bounds)+glm::vec2(1))*glm::vec2(renderer.size())*.5f;require(glm::length(actual-start)<.01f,"inline color bytes consume right-aligned displayed text width");
            renderer.flushUploads();render({},{},result);size_t red=0,green=0;
            for(int y=int(start.y);y<int(start.y)+int(font->height);++y)for(int x=int(start.x);x<int(start.x+used);++x){auto p=pixel(file,x,y);red+=p[0]>250 && p[1]<5 && p[2]<5;green+=p[1]>250 && p[0]<5 && p[2]<5;}require(red>10 && green>10,"inline color and reset glyphs do not reach actual Vulkan pixels");
            auto& text=menu.artwork.front();text.text="\x81 ff800040M";result=hud.buildMenu(renderer.size(),menu,{});require(result.size()==2 && std::abs(result[1].color.g-.2158605f)<.00001f && std::abs(result[1].color.a-64.f/255)<.00001f,"inline color RRGGBBAA does not preserve sRGB channels and independent alpha");
            text.text="\x81 BADVALUEW";result=hud.buildMenu(renderer.size(),menu,{});require(result.size()==2 && result[1].color==glm::vec4(0,1,0,1),"invalid inline color changes the supplied color or draws its payload");
            text.text="\x81 short";require(hud.buildMenu(renderer.size(),menu,{}).size()==1,"truncated inline color draws payload bytes or reads beyond its string");
            text.textMode=OriginalControl::TextMode::Edit;text.editFocused=true;text.editLastTyped=uiTime();text.justify=0;text.text="\x81 FF0000FFM\x82 W";result=hud.buildMenu(renderer.size(),menu,{});require(result.size()==4 && result.back().color==glm::vec4(0,1,0,1),"EditPane cursor inherits inline text color instead of its own text color");
            auto cursor=(glm::vec2(result.back().bounds)+glm::vec2(1))*glm::vec2(renderer.size())*.5f;require(glm::length(cursor-layout.pixel({101+used,101}))<.01f,"inline controls inflate EditPane cursor position");
            text.editPassword=true;result=hud.buildMenu(renderer.size(),menu,{});require(result.size()==text.text.size()+2,"password masking exposes or interprets inline control bytes");
            auto cached=renderer.resources();for(int i=0;i<64;++i)hud.buildMenu(renderer.size(),menu,{});require(renderer.resources().textures==cached.textures,"inline color changes create font textures or upload text spans");
            GameState state;state.playerSide=0;state.sideColors[0]={0,1,0,1};state.ship="\x81 FF0000FFW\x82 W";auto quads=hud.build(renderer.size(),state,{},projection,{},false,false);require(std::any_of(quads.begin(),quads.end(),[](const auto& q){return q.texture!=hudLine && q.color==glm::vec4(1,0,0,1);}),"compiled authored HUD text ignores inline colors");
            puts("Original inline color GPU pixels: exact glyph colors/reset/alignment, sRGB plus alpha, malformed/truncated controls, edit cursor/password, compiled HUD and retained textures passed");
        }
        {
            SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
            Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
            GameStatePane standings(assets);standings.toggle(game);auto size=renderer.size();auto menu=standings.view(size,1);menu.slide->started=uiTime()-1;
            auto draw=[&]{auto overlay=hud.buildMenu(size,menu,{-1,-1});std::vector<HudQuad> quads{{{-1,-1,1,1},{0,0,1,1},{.04f,.1f,.2f,1},0}};quads.insert(quads.end(),overlay.begin(),overlay.end());renderer.flushUploads();render({},{},quads);};draw();
            auto header=std::find_if(menu.artwork.begin(),menu.artwork.end(),[](const auto& c){return c.name=="gamestateheaderbmp";});
            auto close=std::find_if(menu.artwork.begin(),menu.artwork.end(),[](const auto& c){return c.kind==OriginalControl::Kind::Button;});require(header!=menu.artwork.end() && close!=menu.artwork.end(),"Game State bitmap fixture missing");
            require(close->bounds.y==header->bounds.y+4,"Game State close button loses authored 20-pixel image translation");auto background=pixel(file,2,2);size_t keyed=0,opaque=0;
            for(int y=0;y<int(close->bounds.w);++y)for(int x=0;x<int(close->bounds.z);++x){
                auto i=(size_t(y)*close->image->width+x)*4;std::array<int,3> expected;
                if(close->image->pixels[i+3]){for(int c=0;c<3;++c)expected[c]=close->image->pixels[i+c];++opaque;}
                else {auto hx=int(close->bounds.x-header->bounds.x)+x,hy=int(close->bounds.y-header->bounds.y)+y;require(hx>=0 && hx<int(header->image->width) && hy<int(header->image->height),"Game State keyed probe outside header");
                    auto j=(size_t(hy)*header->image->width+hx)*4;expected=background;if(header->image->pixels[j+3])for(int c=0;c<3;++c)expected[c]=header->image->pixels[j+c];++keyed;}
                auto actual=pixel(file,int(close->bounds.x)+x,int(close->bounds.y)+y);for(int c=0;c<3;++c)require(std::abs(actual[c]-expected[c])<=1,"Game State close bitmap placement or BlackPane color key differs");
            }require(keyed>10 && opaque>10,"Game State button probes miss cutouts or opaque artwork");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-game-state.ppm",std::filesystem::copy_options::overwrite_existing);
            PlayerPane pilots(assets);pilots.toggle(game);menu=pilots.view(size,1);menu.slide->started=uiTime()-1;draw();
            auto panel=std::find_if(menu.artwork.begin(),menu.artwork.end(),[](const auto& c){return c.name=="expandedTeamPane" && c.image;});require(panel!=menu.artwork.end(),"expanded player panel fixture missing");
            size_t cutouts=0;for(int y=1;y<int(panel->bounds.w)-1;++y){auto i=size_t(y)*panel->image->width*4;auto actual=pixel(file,int(panel->bounds.x),int(panel->bounds.y)+y);
                std::array<int,3> expected=background;if(panel->image->pixels[i+3])for(int c=0;c<3;++c)expected[c]=panel->image->pixels[i+c];else ++cutouts;
                for(int c=0;c<3;++c)require(std::abs(actual[c]-expected[c])<=1,"F6 authored screen-right bitmap anchor or console color key differs");}
            require(cutouts>16,"F6 bitmap probe misses its keyed upper border");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-player-pane.ppm",std::filesystem::copy_options::overwrite_existing);
            pilots.toggle(game,true);menu=pilots.view(size,1);menu.slide->started=uiTime()-1;draw();
            auto caption=std::find_if(menu.artwork.begin(),menu.artwork.end(),[](const auto& c){return c.name=="playerTeam";});require(caption!=menu.artwork.end() && caption->bounds.z==109,"compact player combo fixture missing its face");
            auto color=Allegiance::PlayerSideColor(pilots.team());auto rgb=pixel(file,int(caption->bounds.x+107),int(caption->bounds.y+11));
            for(int c=0;c<3;++c)require(std::abs(rgb[c]-int(color[c]*255))<=1,"compact team combo does not paint the original faction-colored face");
            auto hit=std::find_if(menu.items.begin(),menu.items.end(),[&](const auto& c){return c.action.op==MenuOp::SelectTeam && c.bounds.x==caption->bounds.x && c.bounds.y==caption->bounds.y;});require(hit!=menu.items.end() && hit->bounds.z==125,"compact team combo clips its click region to its smaller face");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-player-combo.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original Game State/F6 pixels: dynamic group alignment, close-button cutouts and authored right-side roster bitmap passed");
        }
        {
            ConsolePane console(assets);GameState state;state.playerSide=0;state.sideColors[0]={0,1,0,1};
            state.commands[0]={5,1,0,"goto Garrison"};state.commands[1]={3,0,7,"defend Scout"};
            GameMessage tip{};tip.serial=1;tip.text="Original critical tip";tip.warning=true;tip.tip=true;tip.displayTime=1;state.messages.push_back(tip);
            tip.serial=2;tip.text="Original ordinary tip";tip.warning=false;state.messages.push_back(tip);
            auto size=renderer.size();auto draw=[&](double now){
                auto overlay=hud.buildMenu(size,console.view(size,1,state,7,false,true,now),{});
                std::vector<HudQuad> quads{{{-1,-1,1,1},{0,0,1,1},{.04f,.1f,.2f,1},0}};quads.insert(quads.end(),overlay.begin(),overlay.end());renderer.flushUploads();render({},{},quads);
            };draw(1);auto background=pixel(file,2,2);size_t orders=0,critical=0,ordinary=0;
            auto font=assets.font("medVerdana");
            for(int y=57;y<57+font->height;++y)for(int x=57;x<297;++x){auto p=pixel(file,x,y);if(p[0]>background[0]+30){require(std::abs(p[1]-background[1])<=1 && std::abs(p[2]-background[2])<=1,"queued order glyphs darken the underlying world");++orders;}}
            font=assets.font("medBoldVerdana");
            for(int y=134;y<134+font->height;++y)for(int x=size.x/2-310;x<size.x/2+310;++x){auto p=pixel(file,x,y);if(p[0]>background[0]+30 && p[1]<10)++critical;}
            for(int y=size.y-14;y<size.y;++y)for(int x=size.x/2-300;x<size.x/2+300;++x){auto p=pixel(file,x,y);if(p[1]>background[1]+30 && p[0]<10)++ordinary;}
            require(orders>20 && critical>20 && ordinary>20,"original console order/tip glyphs missing, misplaced or recolored");
            auto read=[&](){std::ifstream in(file,std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(in),{});};auto first=read();draw(1.1);require(read()==first,"unchanged cached console changes pixels");
            draw(8.5);size_t remaining=0;
            for(int y=134;y<134+font->height;++y)for(int x=size.x/2-310;x<size.x/2+310;++x)if(pixel(file,x,y)!=background)++remaining;
            require(remaining==0,"expired original critical tip remains rendered");
            state.ballot={3,0,39,"Veteran has proposed offering a draw.  Press [Y] to vote yes, [N] to vote no."};draw(9);
            size_t ballotPixels=0;
            for(int y=169;y<169+2*font->height;++y)for(int x=size.x/2-310;x<size.x/2+310;++x){auto p=pixel(file,x,y);if(p[0]>180 && p[1]>180 && p[2]>180)++ballotPixels;}
            require(ballotPixels>150,"original centered white ballot prompt glyphs are missing or misplaced");
            first=read();draw(9.1);require(read()==first,"retained ballot prompt changes pixels between frames");
            state.ballot={};draw(10);remaining=0;
            for(int y=169;y<169+2*font->height;++y)for(int x=size.x/2-310;x<size.x/2+310;++x)if(pixel(file,x,y)!=background)++remaining;
            require(remaining==0,"voted/expired ballot prompt remains rendered");
            Hud classic(renderer,assets);classic.configure(settings);auto before=classic.build(size,state,{},projection,{},false,false);std::vector<HudQuad> withoutChat(before.begin(),before.end());
            require(classic.messageLayoutStats().rows==0,"original tips are duplicated in ordinary chat history");
            state.moneyRequest={7,125,0,"Requester","Requester has requested $125"};auto after=classic.build(size,state,{},projection,{},false,false);
            require(after.size()==withoutChat.size(),"default classic HUD retains the added centered money-request banner");
            GameMessage chat{};chat.serial=3;chat.text="Separate original chat";chat.chat=true;chat.color={1,0,1,1};state.messages.push_back(chat);
            ConsolePane combined(assets);classic.build(size,state,{},projection,{},false,false);
            auto mixed=classic.buildMenu(size,combined.view(size,1,state,7,false,true,1),{},true);size_t glyphs=0;
            for(const auto& q:mixed)if(q.color==chat.color){float y=(q.bounds.y+1)*size.y*.5f;require(y>=0 && y<30,"classic chat uses the bottom anchor as its top and overlaps critical tips");++glyphs;}
            require(glyphs>10,"combined classic chat/tip fixture has no chat glyphs");
            renderer.flushUploads();render({},{},mixed);
            size_t magenta=0,warning=0;
            for(int y=0;y<30;++y)for(int x=size.x/2-157;x<size.x/2+200;++x){auto p=pixel(file,x,y);if(p[0]>150 && p[2]>150 && p[1]<30)++magenta;}
            for(int y=134;y<134+font->height;++y)for(int x=size.x/2-310;x<size.x/2+310;++x){auto p=pixel(file,x,y);if(p[0]>150 && p[1]<30 && p[2]<30)++warning;}
            require(magenta>20 && warning>20,"original chat and critical tips obscure one another");
            for(int i=4;i<30;++i){chat.serial=i;state.messages.push_back(chat);}
            for(int fontSize:{1,2,3})for(int lines:{1,6,7,20})for(bool loadout:{false,true}){auto preferences=settings;preferences.chatLines=lines;preferences.chatSize=fontSize;classic.configure(preferences);
                auto clipped=classic.build(size,state,{},projection,{},false,false,false,{-1,-1},nullptr,nullptr,{},loadout?ofInLoadout:0);size_t visible=0;
                int height=int(assets.font(fontSize==1?"smallBoldFont":fontSize==2?"largeBoldFont":"hugeBoldFont")->height),listHeight=(loadout?std::min(lines,6):lines)*height+8;
                auto stats=classic.messageLayoutStats();float top=126.f-assets.image("CortUI/common/concommdisplay.png")->height;float visibleHeight=std::min(float(listHeight),float(assets.image("CortUI/common/concommdisplay.png")->height));
                require(stats.pageRows==size_t(listHeight/height) && stats.listBounds==glm::vec4(size.x/2-160,top,355,visibleHeight),"chat list ignores authored padding, font size or full CommPane selection");
                for(const auto& q:clipped)if(q.color==chat.color){float y=(q.bounds.y+1)*size.y*.5f,bottom=(q.bounds.w+1)*size.y*.5f;
                    require(y>=top-.01f && bottom<=top+visibleHeight+.01f,"chat glyphs escape the original list surface");++visible;}
                require(visible>10,"original chat geometry discards the entire history");
            }
            auto preferences=settings;preferences.chatLines=7;preferences.chatSize=1;classic.configure(preferences);
            MenuView docked;docked.original=true;docked.canvas={800,600};classic.buildMenu(size,docked,{-1,-1});
            auto dockedChat=classic.appendMessages(size,state,ofInLoadout);
            auto smallHeight=assets.font("smallBoldFont")->height;
            require(classic.messageLayoutStats().pageRows==(6*smallHeight+8)/smallHeight && std::count_if(dockedChat.begin(),dockedChat.end(),[&](const auto& q){return q.color==chat.color;})>10,"original docked loadout loses chat glyphs or its six-line cap/padding");
            auto updates=classic.messageLayoutStats().updates;
            classic.buildMenu(size,docked,{-1,-1});classic.appendMessages(size,state,ofInLoadout);
            require(classic.messageLayoutStats().updates==updates,"unchanged docked chat repeats text wrapping/encoding");
            classic.buildMenu(size,docked,{-1,-1});classic.appendMessages(size,state);
            require(classic.messageLayoutStats().pageRows==(7*smallHeight+8)/smallHeight,"leaving loadout loses original requested chat lines/padding");
            classic.configure(settings);classic.build(size,state,{},projection,{},false,false);
            auto transparent=classic.build(size,GameState{}, {},projection,{},false,false);std::vector<HudQuad> blank(transparent.begin(),transparent.end());
            renderer.flushUploads();render({},{},blank);
            require(pixel(file,size.x/2,10)==pixel(file,2,2),"original black-key chat background obscures the scene");
            classic.binding(Binding::HudVisible);classic.build(size,state,{},projection,{},false,false);
            require(classic.messageLayoutStats().listBounds==glm::vec4(0),"hidden HUD retains a displayed chat list");
            puts("Original command/tip pixels, additive orders, independent expiry, cache stability and absence of added banner/chat duplicates passed");
        }
        {
            Frontend details(settings);details.originalInterface(assets);details.setPlaying(true);GameState state;state.sideActive=true;
            Allegiance::MissionDetails info;info.name="Rendered mission";info.server="Rendered server";info.core="Active core";info.pods=true;
            details.missionDetailsService([&]{return info;});details.open(Screen::Hangar,&state);details.open(Screen::Pause,&state);details.key(SDLK_I,SDL_SCANCODE_I);
            require(details.screen()==Screen::GameDetails && details.view().overlay,"original Game details render fixture is not a popup over flight");
            auto size=renderer.size();auto layout=originalMenuLayout(size,details.view().classicScale,details.view().canvas);
            auto image=assets.image("dialogbkgndbmp",false);
            auto origin=glm::floor((glm::vec2(800,600)-glm::vec2(image->width,image->height))*.5f);
            MenuView covered;covered.original=covered.overlay=true;covered.canvas=details.view().canvas;covered.classicScale=details.view().classicScale;
            OriginalControl other;other.kind=OriginalControl::Kind::Rectangle;other.bounds={origin,image->width,image->height};other.color={0,1,0,1};covered.artwork.push_back(other);
            auto draw=[&]{hud.buildMenu(size,details.sceneView(),{});hud.buildMenu(size,covered,{},true);
                auto menu=hud.buildMenu(size,details.detailsView(),{},true);std::vector<HudQuad> quads{{{-1,-1,1,1},{0,0,1,1},{.04f,.1f,.2f,1},0}};
                quads.insert(quads.end(),menu.begin(),menu.end());renderer.flushUploads();render({},{},quads);};
            draw();auto world=pixel(file,2,2);
            auto sample=layout.pixel(origin+glm::vec2(5,5));auto expected=&image->pixels[(5*image->width+5)*4];auto actual=pixel(file,int(sample.x),int(sample.y));
            require(actual[0]==expected[0] && actual[1]==expected[1] && actual[2]==expected[2],"Game details substitutes or color-keys the authored MML background");
            size_t yellow=0,white=0;auto page=layout.pixel(origin+glm::vec2(20,15));
            for(int y=int(page.y);y<int(page.y)+40;++y)for(int x=int(page.x);x<int(page.x)+200;++x){auto p=pixel(file,x,y);if(p[0]>180 && p[1]>180 && p[2]<40)++yellow;if(p[0]>180 && p[1]>180 && p[2]>180)++white;}
            require(yellow>20 && white>20,"original Game details title, font, color or report text pixels missing");
            auto below=std::find_if(details.view().items.begin(),details.view().items.end(),[](const auto& i){return i.action.op==MenuOp::ScrollPage && i.action.value==-7 && i.action.delta==1;});
            require(below!=details.view().items.end(),"original Game details page-down trough missing");details.activate(size_t(below-details.view().items.begin()));draw();
            require(pixel(file,2,2)==world,"scrolling the mission popup destroys the underlying flight scene");
            require(std::any_of(details.view().artwork.begin(),details.view().artwork.end(),[](const auto& c){return c.name=="detailsText" && c.text=="Experimental: no ";}),"scrolled original Game details loses its last field");
            puts("Original Game details pixels: centered unkeyed artwork, bitmap title/report, scrolling, top-popup composition and underlying flight scene passed");
            details.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);state.networked=true;state.shipId=42;state.networkPilots={{42,0,"Self"},{43,1,"Opponent"}};
            std::array<Allegiance::PlayerConnectionInfo,2> connections{{{42,120,1},{43,330,20}}};details.connectionInfo(connections,state);draw();
            size_t green=0,cyan=0,red=0;yellow=white=0;
            for(int y=int(page.y);y<int(page.y)+220;++y)for(int x=int(page.x);x<int(page.x)+450;++x){auto p=pixel(file,x,y);
                if(p[0]<40 && p[1]>180 && p[2]<40)++green;if(p[0]<40 && p[1]>180 && p[2]>180)++cyan;if(p[0]>180 && p[1]<40 && p[2]<40)++red;
                if(p[0]>180 && p[1]>180 && p[2]<40)++yellow;if(p[0]>180 && p[1]>180 && p[2]>180)++white;
            }
            require(green>10 && cyan>20 && red>10 && yellow>20 && white>20,"Player Pings loses authored bitmap text or original connection-quality colors");
            require(pixel(file,2,2)==world,"Player Pings popup replaces the flight scene");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-player-pings.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original Player Pings pixels: shared MML panel, heading/self/quality colors, bitmap fonts and flight composition passed");
        }
        {
            CrewPane crew(assets);GameState aboard;aboard.passenger=true;aboard.turret=0;aboard.sourcePilot="Gunship pilot";aboard.playerSide=0;aboard.sideColors[0]={0,1,0,1};
            auto size=renderer.size();auto flight=hud.buildMenu(size,crew.view(size,1,aboard,false,true),{});
            std::vector<HudQuad> quads{{{-1,-1,1,1},{0,0,1,1},{.04f,.1f,.2f,1},0}};
            renderer.flushUploads();auto background=render({},{},quads);
            require(!flight.empty() && std::all_of(flight.begin(),flight.end(),[](auto& q){return q.texture&hudAdditive;}),"crew flight labels lost original additive blending");
            quads.insert(quads.end(),flight.begin(),flight.end());renderer.flushUploads();render({},{},quads);
            auto font=assets.font("medVerdana");size_t glyphPixels=0;
            for(int y=89;y<89+font->height;++y)for(int x=size.x-180;x<size.x-20;++x){auto p=pixel(file,x,y);
                if(p[0]>background[0]+50){require(std::abs(p[1]-background[1])<=1 && std::abs(p[2]-background[2])<=1,"additive red turret text darkens the underlying flight scene");++glyphPixels;}}
            require(glyphPixels>20,"original turret pilot glyphs are invisible or misplaced");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-crew-flight.ppm",std::filesystem::copy_options::overwrite_existing);
            auto empty=render({},{},{});auto& loadout=crew.view(size,1,aboard,true,true);quads.clear();auto overlay=hud.buildMenu(size,loadout,{});quads.assign(overlay.begin(),overlay.end());renderer.flushUploads();render({},{},quads);
            auto& bitmap=loadout.artwork.front();require(bitmap.image && bitmap.name=="InTurretPaneImage","crew loadout bitmap fixture missing");
            for(int y=0;y<int(bitmap.image->height);++y)for(int x=0;x<int(bitmap.image->width);++x){auto p=pixel(file,int(bitmap.bounds.x)+x,int(bitmap.bounds.y)+y);auto i=(size_t(y)*bitmap.image->width+x)*4;
                for(int c=0;c<3;++c)require(std::abs(p[c]-(bitmap.image->pixels[i+3]?bitmap.image->pixels[i+c]:empty[c]))<=1,"original crew loadout PaneImage key, color, placement or sampling differs");}
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-crew-loadout.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original crew pixels: additive parent-name flight labels and exact black-keyed loadout PaneImage passed");
        }
        {
            Settings preferences;Frontend frontend(preferences);frontend.originalInterface(assets);
            SessionOptions session;session.scenario=Scenario::Conquest;frontend.networkSession(session);frontend.setPlaying(true);
            GameState lobby;lobby.networked=lobby.networkLobby=lobby.commander=true;
            lobby.networkPilots={{7,0,"Commander",true,true,true,true,false},{91,0,"Pilot",true,false,false,false,false}};
            frontend.open(Screen::RemoteLobby,&lobby);
            auto row=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.action.op==MenuOp::RosterSelect && item.action.delta==91;});
            require(row!=frontend.view().items.end(),"original player popup has no roster target");
            frontend.pointerMove({500,250});frontend.pointerContext(size_t(row-frontend.view().items.begin()));
            auto leader=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.action.op==MenuOp::MakeLeader && item.popupLevel>=0;});
            require(leader!=frontend.view().items.end(),"original player popup loses its leader action");
            auto selected=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.action.op==MenuOp::LobbyAction && item.action.value==int(LobbyAction::Boot) && item.popupLevel>=0;});
            require(selected!=frontend.view().items.end(),"original first Boot row is missing from player popup");
            auto highlight=selected->bounds;
            auto popup=hud.buildMenu(renderer.size(),frontend.view(),{});std::vector<HudQuad> copy(popup.begin(),popup.end());
            renderer.flushUploads();render({},{},copy);
            auto p=originalMenuLayout(renderer.size()).pixel(glm::vec2(highlight)+glm::vec2(3,5));
            auto color=pixel(file,int(p.x),int(p.y));
            require(color[0]>150 && color[1]<30 && color[2]>25 && color[2]<65,"original player popup is not composited above the team screen with its authored highlight color");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-player-popup.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original player context popup pixels, placement and team-screen composition passed");
        }
        {
            Settings preferences;Frontend frontend(preferences);frontend.originalInterface(assets);
            SessionOptions session;session.scenario=Scenario::Conquest;session.rules.teams=4;session.alliances={0,0,-1,-1,-1,-1};frontend.networkSession(session);
            GameState lobby;lobby.networked=lobby.networkLobby=lobby.networkHost=lobby.commander=true;
            lobby.networkPilots={{7,0,"Owner",true,true,true,true,false},{91,1,"Allied commander",true,false,true,false,false}};
            frontend.open(Screen::RemoteLobby,&lobby);
            auto row=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.control=="team2";});
            require(row!=frontend.view().items.end(),"original alliance popup has no team target");
            frontend.pointerMove({250,210});frontend.pointerContext(size_t(row-frontend.view().items.begin()));
            auto group=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.popupLevel>=0 && item.label=="Ally with Teams 1,2";});
            require(group!=frontend.view().items.end(),"original alliance group caption missing");
            frontend.pointerHover(size_t(group-frontend.view().items.begin()));
            group=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.popupLevel>=0 && item.label=="Ally with Teams 1,2";});
            auto highlight=group->bounds;require(highlight.y<210 && highlight.x>=250,"original alliance menu lost its point anchor");
            auto popup=hud.buildMenu(renderer.size(),frontend.view(),{});std::vector<HudQuad> copy(popup.begin(),popup.end());renderer.flushUploads();render({},{},copy);
            auto p=originalMenuLayout(renderer.size()).pixel(glm::vec2(highlight)+glm::vec2(3,5));auto color=pixel(file,int(p.x),int(p.y));
            require(color[0]>150 && color[1]<30 && color[2]>25 && color[2]<65,"original alliance menu highlight/composition differs from MenuItemPane");
            require(std::any_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[&](const auto& image){return image.image==assets.image("green_edgebmp",true);}),"team painter drops original alliance edge artwork");
            auto resources=renderer.resources();hud.buildMenu(renderer.size(),frontend.view(),{});require(renderer.resources().textures==resources.textures,"resident alliance menu duplicates texture assets");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-alliance-popup.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original alliance pixels: solo/group menu, point anchor, bitmap caption/highlight, team-edge artwork, composition and resident textures passed");
        }
        {
            Settings preferences;Frontend frontend(preferences);frontend.originalInterface(assets);
            SessionOptions session;session.scenario=Scenario::Conquest;frontend.networkSession(session);frontend.open(Screen::Scenarios);frontend.open(Screen::Lobby);
            // Acquire before creating quads for the actual drawable.
            for(int retry=0;!renderer.beginFrame();++retry){SDL_Event event;while(SDL_PollEvent(&event)){};require(retry<500,"could not acquire setup resize frame");SDL_Delay(2);}
            auto quads=hud.buildMenu(renderer.size(),frontend.view(),{});renderer.flushUploads();renderer.draw(projection,{},quads,file.string());
            auto layout=originalMenuLayout(renderer.size());require(layout.scale==1,"original setup is enlarged to fill the window");
            auto image=assets.image("newgamescreen.png",false);require(image->width==800 && image->height==600,"game settings background dimensions differ from authored canvas");
            for(glm::ivec2 at:std::array{glm::ivec2(400,580),glm::ivec2(40,540),glm::ivec2(790,30)}){
                auto p=glm::ivec2(layout.pixel(glm::vec2(at)));auto color=pixel(file,p.x,p.y);auto offset=(size_t(at.y)*image->width+at.x)*4;
                for(int channel=0;channel<3;++channel)require(std::abs(color[channel]-int(image->pixels[offset+channel]))<=1,"game settings changes original background pixels or placement");
            }
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-game-settings.ppm",std::filesystem::copy_options::overwrite_existing);
            auto combo=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.control=="gameTypeComboPane";});require(combo!=frontend.view().items.end(),"game type combo is missing");
            auto b=combo->bounds;frontend.pointerMove(glm::vec2(b)+glm::vec2(4));frontend.pointerPress(size_t(combo-frontend.view().items.begin()));
            quads=hud.buildMenu(renderer.size(),frontend.view(),{});renderer.flushUploads();render({},{},quads);
            auto choice=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.action.op==MenuOp::SetupChoice && item.action.delta==0;});require(choice!=frontend.view().items.end(),"game type popup has no Conquest row");
            auto p=glm::ivec2(originalMenuLayout(renderer.size()).pixel(glm::vec2(choice->bounds)+glm::vec2(3,5)));auto color=pixel(file,p.x,p.y);
            require(color[0]>150 && color[1]<30 && color[2]>25 && color[2]<65,"game setup popup selection does not use the original menu palette");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-game-settings-popup.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original game setup pixels: native canvas, unchanged background and combo composition passed");
        }
        {
            Settings settings;Frontend frontend(settings);frontend.originalInterface(assets);frontend.open(Screen::Games);
            auto item=[&](MenuOp op,int value=0){for(size_t i=0;i<frontend.view().items.size();++i){const auto& entry=frontend.view().items[i];if(entry.action.op==op && entry.action.value==value)return i;}throw std::runtime_error("Games List popup fixture missing");};
            frontend.activate(item(MenuOp::GameFilter));auto face=frontend.view().items[item(MenuOp::GameFilterCombo)].bounds;auto anchor=glm::vec2(face)+glm::vec2(3);frontend.pointerMove(anchor);frontend.pointerPress(item(MenuOp::GameFilterCombo));
            auto draw=[&]{auto quads=hud.buildMenu(renderer.size(),frontend.view(),{-1,-1});renderer.flushUploads();render({},{},quads);};draw();
            auto layout=originalMenuLayout(renderer.size());auto font=assets.font("smallFont");float labelWidth=0;for(unsigned char c:std::string_view("DontCare"))labelWidth+=font->glyphs[c].z;
            auto rows=std::vector<MenuItem>{};for(const auto& row:frontend.view().items)if(row.action.op==MenuOp::GameFilterChoice)rows.push_back(row);
            require(rows.size()==3 && rows[0].action.delta==1 && rows[1].action.delta==0 && rows[2].action.delta==2,"filter menu rows differ from original AddItem order");
            require(rows[0].bounds.z==11+labelWidth+2+font->glyphs[' '].z+9+2 && rows[0].bounds.w==font->height+2 && rows[2].bounds.y+rows[2].bounds.w+2==anchor.y,"filter menu ignores original menu dimensions, authored font or point anchor");
            auto colorAt=[&](glm::vec2 p){auto xy=glm::ivec2(layout.pixel(p));return pixel(file,xy.x,xy.y);};
            for(size_t i=0;i<rows.size();++i){auto actual=colorAt(glm::vec2(rows[i].bounds)+glm::vec2(3,5));std::array<int,3> expected=i?std::array{33,20,30}:std::array{197,0,41};for(int c=0;c<3;++c)require(std::abs(actual[c]-expected[c])<=1,"filter menu loses original selected/unselected palette");}
            require(colorAt(glm::vec2(rows[0].bounds)-glm::vec2(1))==std::array{255,255,255},"filter menu lacks raised white top-left edge");
            auto label=std::find_if(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text=="Yes";});require(label!=frontend.view().artwork.end() && label->font=="smallFont" && label->bounds.x==rows[0].bounds.x+12 && label->bounds.y==rows[0].bounds.y+1,"filter popup overrides its authored label font or offsets");
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-game-filter-popup.ppm",std::filesystem::copy_options::overwrite_existing);
            auto textures=renderer.resources().textures;for(int i=0;i<64;++i)hud.buildMenu(renderer.size(),frontend.view(),{});require(renderer.resources().textures==textures,"resident filter combo creates per-frame textures");
            frontend.key(SDLK_DOWN,SDL_SCANCODE_DOWN);draw();require(colorAt(glm::vec2(rows[1].bounds)+glm::vec2(3,5))==std::array{197,0,41},"filter keyboard highlight does not match painted row");
            frontend.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(frontend.wantsText(),"rendered filter popup dismissal loses original edit focus");
            puts("Original filter combo pixels: authored font, AddItem order, point anchor, menu dimensions, palette, raised border, keyboard highlight and resident textures passed");
        }
        {
            Settings settings;Frontend frontend(settings);frontend.originalInterface(assets);frontend.setPlaying(true);GameState state;state.complete=true;state.playerSide=0;state.result="The Green Team is victorious!";state.endgame.frozen=true;state.endgame.winner=0;
            for(int side=0;side<2;++side){Allegiance::EndgameTeam row;row.id=side;row.civilization=25;Net::text(row.name,side?"Red Team":"Green Team");row.kills=side?12:20;row.color[0]=side?1:0;row.color[1]=side?0:1;state.endgame.teams.push_back(row);}
            for(int i=0;i<40;++i){Allegiance::EndgamePlayer row;row.id=i;row.side=i%2;row.civilization=25;Net::text(row.name,"Pilot "+std::to_string(i));row.score=100-i;row.kills=i;row.played=3661;row.flags=3;state.endgame.players.push_back(row);}
            frontend.open(Screen::Results,&state);auto quads=hud.buildMenu(renderer.size(),frontend.view(),{});renderer.flushUploads();render({},{},quads);
            auto layout=originalMenuLayout(renderer.size());auto background=assets.image("gameoverscreenbmp",false);
            for(glm::ivec2 at:std::array<glm::ivec2,4>{{{5,5},{30,200},{760,80},{760,500}}}){auto p=glm::ivec2(layout.pixel(glm::vec2(at)));auto color=pixel(file,p.x,p.y);auto offset=(size_t(at.y)*background->width+at.x)*4;for(int c=0;c<3;++c)require(std::abs(color[c]-background->pixels[offset+c])<=1,"postgame background is displaced, keyed or resampled");}
            auto header=std::find_if(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](auto& c){return c.name=="statsListHeader5";});require(header!=frontend.view().artwork.end() && header->kind==OriginalControl::Kind::Button && header->checkedState==1 && header->image,"selected original results header face missing");
            for(int y=0;y<int(header->bounds.w);++y)for(int x=0;x<int(header->bounds.z);++x){auto p=glm::ivec2(layout.pixel(glm::vec2(header->bounds)+glm::vec2(x,y)));auto actual=pixel(file,p.x,p.y);int sx=int(std::lround(header->uv.x*header->image->width))+x,sy=int(std::lround(header->image->height/3.f))+y;auto offset=(size_t(sy)*header->image->width+sx)*4;for(int c=0;c<3;++c)require(std::abs(actual[c]-header->image->pixels[offset+c])<=1,"selected postgame sort header uses wrong original face or crop");}
            std::filesystem::copy_file(file,file.parent_path()/"allegiance-native-postgame.ppm",std::filesystem::copy_options::overwrite_existing);
            puts("Original postgame pixels: exact unkeyed background, bitmap table layout and selected sort-header face passed");
        }
        {
            // Exercise the actual HUD sprite choices, rather than just the
            // converter: rounded panel corners and lettering reveal the world.
            GameState panels;EquipmentState row{};row.kind=PartKind::Weapon;row.selected=true;panels.equipment.push_back(row);
            InventoryRow authoredRow;authoredRow.mdl="invsweapon";authoredRow.selected=true;panels.inventory.push_back(authoredRow);
            auto quads=hud.build(renderer.size(),panels,{},projection,{},false,false);
            DrawItem fullBackdrop=backdrop;fullBackdrop.transform=glm::scale(fullBackdrop.transform,{2,2,1});
            auto expected=render({&fullBackdrop,1},{});
            auto selected=renderer.addTexture(assets.image("CortUI/classic/inventoryselected.png",true),true);
            require(std::none_of(quads.begin(),quads.end(),[&](auto& q){return (q.texture&0x0fffffffu)==selected;}),"classic inventory adds the prototype selected-row background");
            for(const char* name:{"CortUI/classic/inventorytop.png","CortUI/classic/inventorybottom.png",
                                "CortUI/classic/inventorytab.png","CortUI/classic/minimappanelexpanded.png"}) {
                auto source=assets.image(name);uint32_t plain=renderer.addTexture(source,true),keyed=renderer.addTexture(assets.image(name,true),true);renderer.flushUploads();
                auto found=std::find_if(quads.begin(),quads.end(),[&](const auto& q){return q.texture==plain || q.texture==keyed;});
                require(found!=quads.end(),"panel artwork is absent from the actual HUD");
                size_t black=0,brightest=0;int brightness=-1;bool cutout=false;
                for(size_t i=0;i<source->pixels.size();i+=4) {
                    const auto* p=source->pixels.data()+i;
                    if(!(p[0]|p[1]|p[2]) && !cutout){black=i/4;cutout=true;}
                    int value=p[0]+p[1]+p[2];if(value>brightness){brightness=value;brightest=i/4;}
                }
                require(cutout && brightness>100,"panel fixture lacks a black cutout or visible border");
                auto size=renderer.size();int w=int(source->width),h=int(source->height),x=(size.x-w)/2,y=(size.y-h)/2;
                HudQuad panel=*found;panel.bounds={2.f*x/size.x-1,2.f*y/size.y-1,2.f*(x+w)/size.x-1,2.f*(y+h)/size.y-1};
                render({&fullBackdrop,1},{},{&panel,1});
                auto hole=pixel(file,x+int(black%source->width),y+int(black/source->width));
                for(int c=0;c<3;++c)require(std::abs(hole[c]-expected[c])<=3,"panel black cutout obscures the world");
                auto border=pixel(file,x+int(brightest%source->width),y+int(brightest/source->width));
                require(std::abs(border[0]-expected[0])+std::abs(border[1]-expected[1])+std::abs(border[2]-expected[2])>30,"panel border disappeared");
            }
            // This is the blue planet in training_1, through the real poster
            // model and background pipeline, with a bright environment behind it.
            auto planet=assets.model("$poster:nebplnt29bmp");uint32_t planetID=renderer.addModel(planet);renderer.flushUploads();
            fullBackdrop.background=1;DrawItem poster{planetID,glm::translate(glm::mat4(1),{0,0,-1}),glm::vec4(1),true,ModelBlend::Alpha,0,1,false,2};
            std::array<DrawItem,2> sky{fullBackdrop,poster};auto center=render(sky,{});auto size=renderer.size();
            auto corner=pixel(file,size.x/4+3,size.y/4+3);
            for(int c=0;c<3;++c)require(std::abs(corner[c]-expected[c])<=3,"planet black surround obscures the environment");
            require(center!=expected,"planet disk disappeared with its surround");
        }
        const std::array<SectorStar,6> stars{{
            {{0,0,-1},1},{{.3f,.2f,-1},.9f},{{-.5f,-.3f,-1},.8f},
            {{.4f,-.2f,1},.7f},{{1,0,0},.6f},{{-1,0,0},.5f}}};
        GameState game;game.sectorId=3;game.stars=stars;CameraState camera;camera.fieldOfView=80;
        auto checkSky=[&](const CameraState& view,bool points,bool readback) {
            auto size=renderer.size();auto sky=hud.buildBackground(size,game,view);
            auto projection=glm::perspectiveRH_ZO(glm::radians(view.fieldOfView),float(size.x)/size.y,30000.f,.2f);projection[1][1]*=-1;
            auto matrix=projection*glm::lookAtRH(view.position,view.position+view.forward,view.up);
            size_t index=0;
            std::array<glm::ivec2,6> pixels{};
            for(const auto& star:stars) {
                auto clip=matrix*glm::vec4(star.direction,0);
                if(!(clip.w>0 && clip.x>=-clip.w && clip.x<clip.w && clip.y>=-clip.w && clip.y<clip.w))continue;
                require(index<sky.quads.size(),"HUD starfield omits a visible direction");
                auto expected=(glm::vec2(clip)/clip.w+1.f)*.5f*glm::vec2(size);
                const auto& quad=sky.quads[index];auto actual=(glm::vec2(quad.bounds)+1.f)*.5f*glm::vec2(size);
                if(quad.texture&hudLine)actual-=glm::vec2(.5f);
                require(std::abs(actual.x-float(int(expected.x)))<=1 && std::abs(actual.y-float(int(expected.y)))<=1,"HUD starfield disagrees with scene perspective");
                if(points)require(quad.texture==0,"a stationary/reentering star retains an invalid motion streak");
                pixels[index++]={int(std::round(actual.x)),int(std::round(actual.y))};
            }
            require(index==sky.quads.size() && sky.posterOffset==index,"HUD starfield renders behind-camera or out-of-frustum stars");
            if(readback) {
                render({},{},{},sky);
                for(size_t i=0;i<index;++i){auto color=pixel(file,pixels[i].x,pixels[i].y);require(color[0]>100 && color[1]>100 && color[2]>100,"projected star missing from Vulkan pixels");}
            }
        };
        checkSky(camera,true,true);
        camera.position={135,-237,48};checkSky(camera,true,true);
        auto turn=glm::rotate(glm::mat4(1),.12f,{0,1,0})*glm::rotate(glm::mat4(1),-.06f,{1,0,0})*glm::rotate(glm::mat4(1),.08f,{0,0,1});
        camera.forward=glm::vec3(turn*glm::vec4(0,0,-1,0));camera.up=glm::vec3(turn*glm::vec4(0,1,0,0));checkSky(camera,false,false);
        checkSky(camera,true,true);
        camera.forward={0,0,1};camera.up={0,1,0};checkSky(camera,false,false);
        camera.forward={0,0,-1};checkSky(camera,true,true);
        camera.fieldOfView=45;checkSky(camera,false,false);checkSky(camera,true,true);
        auto occludedStar=render({&opaque,1},{},{},hud.buildBackground(renderer.size(),game,camera));
        require(occludedStar[0]>250 && occludedStar[1]<10 && occludedStar[2]<10,"sky starfield draws over foreground geometry");
        {
            game.sectorId=3;game.viewFov=65;game.camera=CameraMode::Cockpit;
            std::vector<Contact> fleet(2048);for(size_t i=0;i<fleet.size();++i){auto& c=fleet[i];c.position={0,0,-2};c.radius=1;c.type=ContactShip;c.id=int(i);c.hull=c.shield=1;}
            auto build=[&](std::span<const Contact> contacts){return hud.build(renderer.size(),game,contacts,projection,{},false,false,true);};
            size_t single=build({fleet.data(),1}).size();size_t crowded=build(fleet).size();
            require(single==crowded,"overlapping ordinary radar markers grow with fleet size");
            fleet.back().selected=true;auto important=build(fleet);
            require(important.size()>crowded,"selected marker lost to crowded ordinary contacts");
            auto action=hud.click(glm::vec2(renderer.size())*.5f);
            require(action && action->command==HudCommand::Target && action->value==2047,"crowded radar click loses selected target priority");
            render({},{},important);
        }
        std::filesystem::remove(file);
        printf("Vulkan pixels: compositing, color-keyed radar sprites, actual HUD panel cutouts, planet poster and perspective stars (rotation, translation, FOV, rear culling and reentry) passed (%d,%d,%d behind; %d,%d,%d front).\n",behind[0],behind[1],behind[2],front[0],front[1],front[2]);
    }catch(const std::exception& error){fprintf(stderr,"render check: %s\n",error.what());return 1;}
}
