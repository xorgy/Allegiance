#pragma once
#include "image_text.hpp"
#include "../test_fixture.hpp"
#include "../training_slides.hpp"
#include "../ui_constants.hpp"
#include <functional>

namespace NativeTest {
// Independent evaluator for the primitives actually used by the installed
// training MDLs. Geometry follows ConstantImage/TranslateImage/StringImage;
// text rows use the frozen original BreakLine, never native image compilation
// or wrapping. Both paths share the asset decoder and final quad renderer.
inline void checkTrainingArtwork(Native::Assets& assets,
    const std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)>& comparePixels={}){
    using namespace Native;
    using E=UiExpression;using P=std::shared_ptr<const E>;
    size_t sequences=0,slides=0,texts=0,rows=0,images=0,keyed=0;
    struct Lesson {int id;const char* file;};
    constexpr Lesson lessons[]={{1,"tm_1_introduction"},{2,"tm_2_basic_flight"},{3,"tm_3_basic_weaponry"},
        {4,"tm_4_enemy_engagement"},{5,"tm_5_command_view"},{6,"tm_6_practice_arena"},{8,"tm_8_nanite"}};
    for(auto lesson:lessons)for(bool post:{false,true}){
        if(lesson.id==1 && !post)continue;
        auto name=std::string(lesson.file)+(post && lesson.id!=1?"_post":"");
        auto script=UiScript::read(assets.find(name+".mdl"));auto actual=loadTrainingSlides(assets,lesson.id,post);
        auto resolve=[&](P p){
            for(int depth=0;p && p->kind==E::Kind::Reference;++depth){
                require(depth<64,"cyclic training reference");auto it=script.definitions.find(p->name);if(it==script.definitions.end())break;p=it->second;
            }return p;
        };
        auto scalar=[&](P p){p=resolve(p);require(bool(p),"empty training scalar");if(p->name=="true")return 1.;if(p->name=="false")return 0.;require(p->kind==E::Kind::Number,"unexpected training scalar primitive");return p->number;};
        auto literal=[&](P p){p=resolve(p);require(p && p->kind==E::Kind::String,"unexpected training string primitive");return p->name;};
        auto list=resolve(script.definitions.at("slides"));require(list && list->kind==E::Kind::List && list->arguments.size()==actual.size(),"native training sequence drops or adds an authored slide");
        for(size_t index=0;index<list->arguments.size();++index){
            auto entry=resolve(list->arguments[index]);require(entry && entry->kind==E::Kind::Tuple && entry->arguments.size()==2,"invalid training reference entry");
            auto cue=resolve(entry->arguments[1]);require(bool(cue),"empty training cue");
            auto sound=originalSoundId(cue->name);require(sound && actual[index].sound==*sound,"training sound order differs from the authored slideshow");
            MenuView expected;expected.original=true;
            std::vector<OriginalControl> blocks;
            std::function<void(P,glm::vec2)> render=[&](P p,glm::vec2 origin){
                p=resolve(p);require(bool(p),"empty training image");
                const auto& args=p->arguments;
                if(p->name=="GroupImage"){
                    auto group=resolve(args.at(0));require(group && group->kind==E::Kind::List,"invalid training image group");
                    for(auto it=group->arguments.rbegin();it!=group->arguments.rend();++it)render(*it,origin);
                }else if(p->name=="TranslateImage"){
                    auto point=resolve(args.at(1));require(point && point->name=="Point" && point->arguments.size()==2,"invalid training translation");
                    render(args.at(0),origin+glm::vec2(scalar(point->arguments[0]),scalar(point->arguments[1])));
                }else if(p->name=="ImportImage"){
                    bool key=scalar(args.at(1))!=0;auto image=assets.image(literal(args.at(0)),key);
                    OriginalControl c;c.kind=OriginalControl::Kind::Image;c.image=image;c.bounds={origin.x,600-origin.y-image->height,image->width,image->height};
                    expected.artwork.push_back(std::move(c));++images;if(key)++keyed;
                }else if(p->name=="StringImage"){
                    auto justification=resolve(args.at(0));require(bool(justification),"missing training text justification");
                    int align=justification->name=="JustifyLeft"?0:justification->name=="JustifyCenter"?1:justification->name=="JustifyRight"?2:-1;
                    require(align>=0,"invalid training text justification");auto symbol=resolve(args.at(2));require(symbol && symbol->kind==E::Kind::Reference,"training font is not a resident symbol");
                    auto color=resolve(args.at(3));require(color && color->name=="white","unexpected installed training text color");
                    auto font=assets.font(symbol->name);int width=int(scalar(args.at(1)));auto value=literal(args.at(4));
                    ReferenceImageText::Font metrics;for(int ch=0;ch<256;++ch)metrics.advances[ch]=font->glyphs[ch].z;
                    ReferenceImageText::StringImage original{&metrics};ReferenceImageText::ZString remaining{value};int y=0;
                    while(!remaining.text.empty()){
                        auto row=original.BreakLine(remaining,width).text;float extent=0;for(unsigned char ch:row)extent+=metrics.advances[ch];
                        int x=align==1?int((width-extent)/2):align==2?int(width-extent):0;
                        OriginalControl c;c.kind=OriginalControl::Kind::Text;c.text=std::move(row);c.font=symbol->name;c.bounds={origin.x+x,600-origin.y+y,0,font->height};
                        c.textMode=OriginalControl::TextMode::Native;c.singleLine=true;c.clipCharacters=false;expected.artwork.push_back(std::move(c));y+=font->height;++rows;
                    }
                    OriginalControl block;block.kind=OriginalControl::Kind::Text;block.text=std::move(value);block.font=symbol->name;block.bounds={origin.x,600-origin.y,width,y};block.justify=align;blocks.push_back(std::move(block));++texts;
                }else require(false,"unhandled installed training image primitive");
            };
            render(entry->arguments[0],{});
            std::vector<const OriginalControl*> actualImages,expectedImages,actualText;
            for(const auto& c:actual[index].view.artwork){if(c.image)actualImages.push_back(&c);else if(c.kind==OriginalControl::Kind::Text)actualText.push_back(&c);else require(false,"unexpected native training control");}
            for(const auto& c:expected.artwork)if(c.image)expectedImages.push_back(&c);
            require(actual[index].view.original && actual[index].view.canvas==glm::ivec2(800,600) && actual[index].view.items.empty() && actual[index].view.text.empty() && !actual[index].view.shipPreview,"training contains replacement UI or a model preview");
            require(actualImages.size()==expectedImages.size() && actualText.size()==blocks.size(),"training loses authored image/text layers");
            for(size_t i=0;i<actualImages.size();++i)require(actualImages[i]->image==expectedImages[i]->image && actualImages[i]->bounds==expectedImages[i]->bounds && actualImages[i]->color==glm::vec4(1) && actualImages[i]->uv==glm::vec4(0,0,1,1) && !actualImages[i]->additive,"training image order, translation, tint, UV or keying differs from the original primitives");
            for(size_t i=0;i<blocks.size();++i){const auto& c=*actualText[i];const auto& b=blocks[i];require(c.text==b.text && c.bounds==b.bounds && c.font==b.font && c.justify==b.justify && c.color==glm::vec4(1) && c.textMode==OriginalControl::TextMode::Image,"training text differs from original StringImage geometry/font/rows");}
            if(comparePixels)comparePixels(actual[index].view,expected,name+"-"+std::to_string(index));++slides;
        }++sequences;
    }
    require(sequences==13 && slides>=70 && texts>=10 && keyed>=10,"training reference coverage is incomplete");
    printf("Original training artwork: %zu sequences, %zu slides, %zu image layers (%zu keyed), %zu text blocks/%zu frozen rows and %zu independent pixel comparisons passed without a Game\n",sequences,slides,images,keyed,texts,rows,comparePixels?slides:0);
}
}
