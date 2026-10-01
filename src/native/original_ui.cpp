#include "original_ui.hpp"
#include "ui_constants.hpp"
#include "../ClientCore/AuthoredText.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <functional>
#include <stdexcept>

namespace Native {
namespace {
using E=UiExpression;using P=std::shared_ptr<const E>;
std::string controlName(std::string name) {
    if(auto split=name.rfind("::");split!=name.npos)name.erase(0,split+2);
    return name;
}
struct Compiler {
    Assets& assets;const UiScript& script;const UiNumbers& numbers;const UiStrings& strings;
    std::vector<OriginalControl> controls;
    const UiSlots* slots{};bool imageColorKeyHint{};
    std::unordered_map<std::string,int> hoverValues;
    P resolve(P e,unsigned depth=0)const {
        if(depth>64)throw std::runtime_error("Cyclic original UI definition");
        if(e && e->kind==E::Kind::Reference)if(auto i=script.definitions.find(e->name);i!=script.definitions.end())return resolve(i->second,depth+1);
        return e;
    }
    P arg(P e,size_t i)const {e=resolve(e);return e && i<e->arguments.size()?e->arguments[i]:P{};}
    double number(P e)const {
        if(e && e->kind==E::Kind::Reference)if(auto i=hoverValues.find(e->name);i!=hoverValues.end())return i->second;
        e=resolve(e);if(!e)return 0;if(e->kind==E::Kind::Number)return e->number;
        if(e->name=="HoverSiteGetID"){
            auto site=arg(e,0);if(site)if(auto i=hoverValues.find(site->name);i!=hoverValues.end())return i->second;
            return number(arg(resolve(site),0));
        }
        if(auto i=numbers.find(e->name);i!=numbers.end())return i->second;
        if(auto id=originalSoundId(e->name))return *id;
        if(e->name=="true")return 1;if(e->name=="ButtonNormal")return 15;
        if(e->name=="ButtonNormalCheckBox")return 255;if(e->name=="Button3State")return 4095;if(e->name=="ButtonFaceUp")return 1;
        if(e->name=="JustifyCenter")return 1;if(e->name=="JustifyRight")return 2;
        if(e->name=="ButtonFaceDown")return 2;
        if(e->name=="PointY" || e->name=="PointX"){
            auto p=resolve(arg(e,0));if(p && p->name=="ImageSize"){auto b=imageBounds(arg(p,0));return e->name=="PointY"?b.w:b.z;}
            auto xy=point(p);return e->name=="PointY"?xy.y:xy.x;
        }
        if(e->name=="Max")return std::max(number(arg(e,0)),number(arg(e,1)));
        if(e->name=="Subtract")return number(arg(e,0))-number(arg(e,1));
        if(e->name=="Add")return number(arg(e,0))+number(arg(e,1));
        if(e->name=="Multiply")return number(arg(e,0))*number(arg(e,1));
        if(e->name=="Divide"){auto denominator=number(arg(e,1));return denominator?number(arg(e,0))/denominator:0;}
        if(e->kind==E::Kind::Operator){auto a=number(arg(e,0)),b=number(arg(e,1));
            if(e->name=="|")return double(uint64_t(a)|uint64_t(b));if(e->name=="&")return double(uint64_t(a)&uint64_t(b));
            if(e->name=="negate")return -a;
            if(e->name=="+")return a+b;if(e->name=="-")return e->arguments.size()==1?-a:a-b;
            if(e->name=="*")return a*b;if(e->name=="/")return b?a/b:0;}
        return 0;
    }
    glm::vec4 controlsBounds(size_t begin,bool includeOrigin=false)const {
        glm::vec2 lo{},hi{};bool first=!includeOrigin;
        for(size_t i=begin;i<controls.size();++i){const auto& c=controls[i];if(c.clip && (c.clip->z<=0 || c.clip->w<=0) && c.bounds.z>0 && c.bounds.w>0)continue;auto b=c.visibleBounds();
            if(first){lo=glm::vec2(b);hi=lo+glm::vec2(b.z,b.w);first=false;}else {lo=glm::min(lo,glm::vec2(b));hi=glm::max(hi,glm::vec2(b)+glm::vec2(b.z,b.w));}}
        return {lo,hi-lo};
    }
    glm::vec4 imageBounds(P e)const {
        Compiler measuring{assets,script,numbers,strings,{},slots,imageColorKeyHint};measuring.imageControls(e,{},"",0);
        return measuring.controlsBounds(0,true);
    }
    P switched(P e,bool pane=false)const {
        e=resolve(e);if(!e)return {};
        if(e->name=="SwitchString" || e->name=="SwitchImage") {
            auto value=number(arg(e,0));auto list=resolve(arg(e,2));
            if(list)for(auto& pair:list->arguments)if(number(arg(pair,0))==value)return switched(arg(pair,1));
            return switched(arg(e,1));
        }
        if(pane && e->name=="SwitchPane") {
            auto value=number(arg(e,0));auto list=resolve(arg(e,1));
            if(list)for(auto& pair:list->arguments)if(number(arg(pair,1))==value)return switched(arg(pair,0),true);
            return {};
        }
        return e;
    }
    std::string string(P e)const {
        e=switched(e);if(!e)return {};if(e->kind==E::Kind::String)return e->name;
        if(auto i=strings.find(e->name);i!=strings.end())return i->second;
        if(e->kind==E::Kind::Operator && e->name=="+")return string(arg(e,0))+string(arg(e,1));
        if(e->name=="Concat" || e->name=="ConcatinatedString")return string(arg(e,0))+string(arg(e,1));
        if(e->name=="NumberString")return std::to_string(int(number(arg(e,0))));
        return {};
    }
    std::string textSite(P e)const {
        e=resolve(e);if(!e || e->name!="SwitchString")return {};
        return hoverSite(arg(e,0));
    }
    std::string hoverSite(P condition)const {
        condition=resolve(condition);if(!condition)return {};
        if(condition->kind==E::Kind::Reference && condition->name=="CurrentHover" && numbers.contains("CurrentHover"))return condition->name;
        if(condition->name!="HoverSiteGetID")return {};
        auto site=arg(condition,0);return site?site->name:std::string{};
    }
    // Compile all authored hover branches at rebuild time. The frame only
    // selects resident controls; it neither substitutes labels nor reads MDL.
    bool hoverBranches(P selector,bool paneSwitch,const std::function<void()>& compile) {
        selector=resolve(selector);if(!selector || selector->name!=(paneSwitch?"SwitchPane":"SwitchString"))return false;
        auto site=hoverSite(arg(selector,0));if(site.empty() || hoverValues.contains(site))return false;
        int initial=int(number(arg(selector,0)));auto list=resolve(arg(selector,paneSwitch?1:2));auto cases=std::make_shared<std::vector<int>>();
        if(list)for(auto& pair:list->arguments)cases->push_back(int(number(arg(pair,paneSwitch?1:0))));
        auto branch=[&](int id,std::optional<int> value){
            hoverValues.emplace(site,id);auto begin=controls.size();compile();hoverValues.erase(site);
            for(size_t i=begin;i<controls.size();++i)controls[i].hoverConditions.push_back({site,value,cases,initial});
        };
        if(!paneSwitch){int other=0;while(std::find(cases->begin(),cases->end(),other)!=cases->end())++other;branch(other,{});}
        for(int id:*cases)branch(id,id);
        return true;
    }
    glm::vec2 point(P e)const {e=resolve(e);return {number(arg(e,0)),number(arg(e,1))};}
    int imageJustification(P e)const {
        e=resolve(e);if(!e)return 0;auto name=controlName(e->name);
        if(name=="JustifyLeft")return 1;if(name=="JustifyRight")return 2;if(name=="JustifyTop")return 4;if(name=="JustifyBottom")return 8;
        if(name=="JustifyXCenter")return 16;if(name=="JustifyYCenter")return 32;if(name=="JustifyCenter")return 48;
        if(name=="|")return imageJustification(arg(e,0))|imageJustification(arg(e,1));
        if(name=="&")return imageJustification(arg(e,0))&imageJustification(arg(e,1));
        return int(number(e));
    }
    glm::vec4 color(P e)const {
        e=resolve(e);if(!e)return glm::vec4(1);
        if(e->name=="Color")return {number(arg(e,0)),number(arg(e,1)),number(arg(e,2)),e->arguments.size()>3?number(arg(e,3)):1};
        if(e->kind==E::Kind::Reference && numbers.contains(e->name+".r")){
            glm::vec4 result(1);const char* channels[]={".r",".g",".b",".a"};
            for(int i=0;i<4;++i)if(auto value=numbers.find(e->name+channels[i]);value!=numbers.end())result[i]=float(value->second);
            return result;
        }
        if(e->kind==E::Kind::Reference)if(auto value=originalColor(controlName(e->name)))return *value;
        return glm::vec4(1);
    }
    void font(OriginalControl& control,P e)const {
        e=resolve(e);
        if(!e){control.font="smallFont";return;}
        if(e->name=="Win32Font"){
            control.font=string(arg(e,0));control.fontHeight=int(number(arg(e,1)));control.fontBold=number(arg(e,2))!=0;
            if(!control.fontHeight || number(arg(e,3)))throw std::runtime_error("Unsupported authored Win32Font height/stretch");
        } else if(e && e->kind==E::Kind::Reference)control.font=controlName(e->name);
        else throw std::runtime_error("Unsupported original UI font: "+(e?e->name:"<default>"));
    }
    std::shared_ptr<ImageData> image(P e) {
        e=switched(e);if(!e)return {};
        if(e->name=="ImportImage" || e->name=="ImportImageFromFile" || e->name=="ImportImageLR" || e->name=="ImportImage3D") {
            auto result=assets.image(string(arg(e,0)),number(arg(e,e->name=="ImportImage3D"?2:1))!=0 || imageColorKeyHint);result->mipmaps=false;return result;
        }
        // The intro's externally supplied background replaces a 3D logo view.
        if(e->name=="bgImage")return image(script.definitions.at("bkImage"));
        if(e->kind==E::Kind::Reference)if(auto it=strings.find(e->name);it!=strings.end() && !it->second.empty())return assets.image(it->second,false);
        return {};
    }
    void children(P list,glm::vec2 origin,unsigned depth) {
        list=resolve(list);if(!list)return;
        for(auto& pair:list->arguments) {
            auto control=arg(pair,0);auto p=origin+point(arg(pair,1));
            std::string name=control && control->kind==E::Kind::Reference?controlName(control->name):std::string{};
            pane(control,p,std::move(name),depth+1);
        }
    }
    // MDL image coordinates grow upward. StringImage has its top at y=0;
    // imported surfaces extend upward from y=0. Compile both into screen quads.
    void imageControls(P e,glm::vec2 origin,const std::string& name,unsigned depth) {
        if(depth>64)throw std::runtime_error("Original image nesting limit");e=switched(e);if(!e)return;
        if(e->name=="StringImage" && hoverBranches(arg(e,4),false,[&]{imageControls(e,origin,name,depth+1);}))return;
        if(slots)if(auto i=slots->find(controlName(e->name));i!=slots->end()){
            controls.push_back({OriginalControl::Kind::Slot,controlName(e->name),{},{},{origin-glm::vec2(0,i->second.y),i->second}});return;
        }
        if(e->name=="JustifyImage") {
            auto begin=controls.size();imageControls(arg(e,0),origin,name,depth+1);if(begin==controls.size())return;
            auto bounds=controlsBounds(begin);auto lo=glm::vec2(bounds),hi=lo+glm::vec2(bounds.z,bounds.w);
            int justification=imageJustification(arg(e,1));glm::vec2 offset{};
            if(justification&1)offset.x=origin.x-lo.x;else if(justification&2)offset.x=origin.x-hi.x;else if(justification&16)offset.x=origin.x-(lo.x+hi.x)*.5f;
            if(justification&8)offset.y=origin.y-hi.y;else if(justification&4)offset.y=origin.y-lo.y;else if(justification&32)offset.y=origin.y-(lo.y+hi.y)*.5f;
            for(size_t i=begin;i<controls.size();++i)controls[i].translate(offset);
            return;
        }
        if(e->name=="TranslateImage"){auto p=point(arg(e,1));imageControls(arg(e,0),origin+glm::vec2(p.x,-p.y),name,depth+1);return;}
        if(e->name=="ClipImage") {
            auto begin=controls.size();imageControls(arg(e,0),origin,name,depth+1);auto rectangle=arg(e,1);
            float left=number(arg(rectangle,0)),bottom=number(arg(rectangle,1)),right=number(arg(rectangle,2)),top=number(arg(rectangle,3));
            for(size_t i=begin;i<controls.size();++i)controls[i].intersectClip({origin+glm::vec2(left,-top),right-left,top-bottom});return;
        }
        if(e->name=="ImportImageLR") {
            // WinTrek translates this imported surface down by its height in
            // image coordinates, putting its top at the supplied screen origin.
            if(auto im=image(e))controls.push_back({OriginalControl::Kind::Image,name,{},{},{origin.x,origin.y,im->width,im->height},glm::vec4(1),im});
            return;
        }
        if(e->name=="UndetectableImage" || e->name=="WrapImage"){imageControls(arg(e,0),origin,name,depth+1);return;}
        if(e->name=="BlendImage") {
            auto begin=controls.size();imageControls(arg(e,0),origin,name,depth+1);
            auto mode=resolve(arg(e,1));bool additive=mode && mode->name=="BlendModeAdd";
            for(size_t i=begin;i<controls.size();++i)controls[i].additive=additive;
            return;
        }
        if(e->name=="GroupImage") {
            auto list=resolve(arg(e,0));if(list)for(auto it=list->arguments.rbegin();it!=list->arguments.rend();++it)imageControls(*it,origin,name,depth+1);return;
        }
        if(e->name=="BlendColorImage"){
            auto begin=controls.size();imageControls(arg(e,1),origin,name,depth+1);
            if(begin==controls.size())return;
            auto bounds=controlsBounds(begin);auto lo=glm::vec2(bounds),hi=lo+glm::vec2(bounds.z,bounds.w);
            auto tint=color(arg(e,0));
            // WinTrek fills the image rectangle with premultiplied color.
            // Native UI quads use source alpha; convert the overlay accordingly.
            if(tint.a>0){for(int axis=0;axis<3;++axis)tint[axis]/=tint.a;
                controls.push_back({OriginalControl::Kind::Rectangle,name,{},{},{lo,hi-lo},tint});}
            return;
        }
        if(e->name=="StringImage") {
            float width=int(number(arg(e,1)));
            OriginalControl c{OriginalControl::Kind::Text,name,string(arg(e,4)),{},{origin.x,origin.y,width,600},color(arg(e,3)),{},0,int(number(arg(e,0)))};
            c.textSite=textSite(arg(e,4));font(c,arg(e,2));auto metrics=assets.font(c.font,c.fontHeight,c.fontBold);
            c.textMode=OriginalControl::TextMode::Image;
            c.bounds.w=metrics->height*Allegiance::ImageTextLines(c.text,int(width),[&](unsigned char ch){return metrics->glyphs[ch].z;});
            controls.push_back(std::move(c));return;
        }
        if(e->name=="GaugeImage") {
            auto im=image(arg(e,1));if(!im)return;float fraction=std::clamp(float(number(arg(e,2))),0.f,1.f);
            float width=std::floor(im->width*fraction);if(width<=0)return;
            OriginalControl c{OriginalControl::Kind::Image,name,{},{},{origin.x,origin.y-im->height,width,im->height},glm::vec4(1),im};c.uv.z=width/im->width;controls.push_back(std::move(c));return;
        }
        if(e->name=="PaneImage") {
            bool previous=imageColorKeyHint;imageColorKeyHint=imageColorKeyHint || number(arg(e,2))!=0;
            auto child=arg(e,0);auto begin=controls.size();auto size=pane(child,{},child && child->kind==E::Kind::Reference?controlName(child->name):name,depth+1);imageColorKeyHint=previous;
            for(size_t i=begin;i<controls.size();++i)controls[i].translate(origin-glm::vec2(0,size.y));return;
        }
        if(auto im=image(e))controls.push_back({OriginalControl::Kind::Image,name,{},{},{origin.x,origin.y-im->height,im->width,im->height},e->name=="ImportImage3D"?color(arg(e,1)):glm::vec4(1),im});
    }
    glm::vec2 pane(P e,glm::vec2 origin,std::string name,unsigned depth=0) {
        auto begin=controls.size();glm::vec2 size{};compilePane(e,origin,std::move(name),depth,size);
        auto type=switched(e,true);
        // ListPane's MDL size is a minimum: its painter can widen it. Hover
        // wrappers are transparent and must retain that same runtime extent.
        bool flexible=type && (type->name=="ListPane" || type->name=="ListWithScrollBarPane" || type->name=="HoverPane" || type->name=="HoverPaneColumns");
        if(!flexible)for(size_t i=begin;i<controls.size();++i)controls[i].intersectClip({origin,size});return size;
    }
    void compilePane(P e,glm::vec2 origin,std::string name,unsigned depth,glm::vec2& extent) {
        if(slots && e && e->kind==E::Kind::Reference)if(auto i=slots->find(controlName(e->name));i!=slots->end()){
            extent=i->second;controls.push_back({OriginalControl::Kind::Slot,controlName(e->name),{},{},{origin,i->second}});return;
        }
        if(depth>64)throw std::runtime_error("Original UI nesting limit");
        if(hoverBranches(e,true,[&]{extent=glm::max(extent,pane(e,origin,name,depth+1));}))return;
        e=switched(e,true);if(!e)return;
        if(e->name=="StringPane" && hoverBranches(arg(e,0),false,[&]{extent=glm::max(extent,pane(e,origin,name,depth+1));}))return;
        if(e->kind==E::Kind::Tuple && e->arguments.size()==1){extent=pane(arg(e,0),origin,std::move(name),depth+1);return;}
        auto add=[&](OriginalControl c){extent=glm::max(extent,glm::vec2(c.bounds)+glm::vec2(c.bounds.z,c.bounds.w));c.name=name;c.translate(origin);controls.push_back(std::move(c));};
        if(e->name=="HoverPane" || e->name=="HoverPaneColumns") {
            auto child=arg(e,2);auto key=child && child->kind==E::Kind::Reference?controlName(child->name):name;auto begin=controls.size();extent=pane(child,origin,key,depth+1);
            auto site=arg(e,0);auto binding=site?site->name:std::string{};P list;
            // Match the actual HoverSite, including imported namespaces. A
            // popup can author a differently named table beside its parent.
            for(auto& [key,value]:script.definitions)if(!binding.empty() && textSite(value)==binding){list=resolve(arg(resolve(value),2));break;}
            if(!list)return;
            auto help=[&](int id){for(auto& pair:list->arguments)if(number(arg(pair,0))==id)return string(arg(pair,1));return std::string{};};
            auto base=int(number(arg(e,1)));auto columns=e->name=="HoverPaneColumns"?resolve(arg(e,3)):P{};
            if(columns){float left=0;for(size_t i=0;i<columns->arguments.size();++i){float right=number(columns->arguments[i]);OriginalControl c;c.kind=OriginalControl::Kind::Hover;c.name="hover/"+key+"/"+std::to_string(i);c.bounds={origin.x+left,origin.y,right-left,extent.y};c.help=help(base+int(i));c.helpSite=binding;c.helpID=base+int(i);auto target=key+std::to_string(i);c.hoverTarget=std::any_of(controls.begin()+begin,controls.end(),[&](const auto& child){return child.name==target;})?target:key;controls.push_back(std::move(c));left=right;}}
            else {auto text=help(base);for(size_t i=begin;i<controls.size();++i){controls[i].help=text;controls[i].helpSite=binding;controls[i].helpID=base;}OriginalControl c;c.kind=OriginalControl::Kind::Hover;c.name="hover/"+key;c.bounds={origin,extent};c.help=std::move(text);c.helpSite=binding;c.helpID=base;c.hoverTarget=key;controls.push_back(std::move(c));}
            return;
        }
        if(e->name=="ScreenPane" || e->name=="ImagePane") {
            if(auto im=image(arg(e,0)))add({OriginalControl::Kind::Image,{},{},{},{0,0,im->width,im->height},glm::vec4(1),im});
            else {
                auto begin=controls.size();imageControls(arg(e,0),{},name,depth+1);auto bounds=controlsBounds(begin,true);auto lo=glm::vec2(bounds);
                for(size_t i=begin;i<controls.size();++i)controls[i].translate(origin-lo);extent={bounds.z,bounds.w};
            }
            children(arg(e,1),origin,depth);return;
        }
        if(e->name=="BlackPane"){
            auto child=arg(e,0);auto childName=child && child->kind==E::Kind::Reference?controlName(child->name):name;
            auto begin=controls.size();extent=pane(child,origin,std::move(childName),depth+1);
            controls.insert(controls.begin()+begin,{OriginalControl::Kind::Rectangle,name,{},{},{origin,extent},{0,0,0,imageColorKeyHint?0.f:1.f}});return;
        }
        if(e->name=="MapPreviewPane") {
            if(auto im=image(arg(e,0)))add({OriginalControl::Kind::Image,{},{},{},{0,0,im->width,im->height},glm::vec4(1),im});
            return;
        }
        if(e->name=="ButtonPane" || e->name=="SoundButtonPane" || e->name=="ButtonEffectPane") {
            auto im=image(arg(e,0));if(!im)return;uint32_t mask=uint32_t(number(arg(e,1)));auto n=std::popcount(mask);
            if(!n || im->height%unsigned(n))throw std::runtime_error("Invalid original button strip: "+name);
            OriginalControl c{OriginalControl::Kind::Button,{},{},{},{0,0,im->width,im->height/unsigned(n)},glm::vec4(1),im,mask};
            c.toggleOnRelease=number(arg(e,2))!=0;
            // ButtonEffectPane and FrameImageButtonPane are engine primitives
            // without Trek's attached sound sinks.
            if(e->name!="ButtonEffectPane"){
                c.hoverSound=mouseoverSound;c.clickSound=e->name=="SoundButtonPane"?SoundID(number(arg(e,3))):mask&0x20?mouseclickSound:positiveButtonClickSound;
            }
            add(std::move(c));return;
        }
        if(e->name=="ButtonBarPane") {
            auto im=image(arg(e,0));if(!im)return;auto mask=uint32_t(number(arg(e,1)));auto count=std::popcount(mask);
            if(!count || im->height%unsigned(count))throw std::runtime_error("Invalid original button bar");
            auto columns=resolve(arg(e,2));float x=0;
            for(size_t i=0;columns && i<columns->arguments.size();++i){float end=float(number(columns->arguments[i]));OriginalControl c{OriginalControl::Kind::Button,{},{},{},{x,0,end-x,im->height/unsigned(count)},glm::vec4(1),im,mask};
                c.name=name+std::to_string(i);c.translate(origin);c.uv={x/im->width,0,end/im->width,1};c.clickSound=mouseclickSound;c.hoverSound=mouseoverSound;controls.push_back(std::move(c));x=end;}extent={x,im->height/unsigned(count)};return;
        }
        if(e->name=="TextButtonBarPane") {
            auto columns=resolve(arg(e,0)),labels=resolve(arg(e,1));float x=0;
            auto height=assets.font("smallFont")->height;
            if(!columns || !labels || columns->arguments.size()!=labels->arguments.size())throw std::runtime_error("Invalid authored text button bar");
            for(size_t i=0;i<columns->arguments.size();++i){float right=number(columns->arguments[i]);
                OriginalControl c;c.kind=OriginalControl::Kind::Button;c.name=name+std::to_string(i);c.text=string(labels->arguments[i]);c.font="smallFont";c.bounds={origin.x+x,origin.y,right-x,height};c.textMode=OriginalControl::TextMode::Pane;c.singleLine=true;c.faces=3;c.clickSound=mouseclickSound;c.hoverSound=mouseoverSound;controls.push_back(std::move(c));x=right;
            }extent={x,height};return;
        }
        if(e->name=="FrameImageButtonPane") {
            // Animated button frames are supplied by the binary MDL reader.
            auto frames=assets.animation(string(arg(e,0)));auto im=frames.front();
            OriginalControl c{OriginalControl::Kind::Button,{},{},{},{0,0,im->width,im->height},glm::vec4(1),im,1};
            c.animation.assign(frames.begin(),frames.end());c.duration=float(number(arg(e,1)));c.pressAction=true;add(std::move(c));return;
        }
        if(e->name=="StringPane") {
            auto size=point(arg(e,2));OriginalControl c{OriginalControl::Kind::Text,{},string(arg(e,0)),{},{0,0,size.x,size.y},color(arg(e,1)),{},0,int(number(arg(e,3)))};
            c.textMode=OriginalControl::TextMode::Pane;c.textSite=textSite(arg(e,0));font(c,arg(e,4));
            if(!arg(e,2)){c.clipCharacters=false;auto metrics=assets.font(c.font,c.fontHeight,c.fontBold);c.bounds.w=metrics->height;c.bounds.z=Allegiance::TextExtent(c.text,[&](unsigned char ch){return metrics->glyphs[ch].z;});}
            c.singleLine=true;if(number(arg(e,5)))c.justify=3;add(std::move(c));return;
        }
        if(e->name=="ImageComboPane") {
            auto im=image(arg(e,0));if(!im)return;
            OriginalControl c{OriginalControl::Kind::Button,{},{},{},{0,0,im->width,im->height/2},glm::vec4(1),im,5};c.widget=OriginalControl::Widget::ImageCombo;c.hoverSound=mouseoverSound;c.selectSound=mouseclickSound;font(c,arg(e,1));add(std::move(c));return;
        }
        if(e->name=="AnimatedImagePane" || e->name=="AnimatedImagePaneRect") {
            auto content=switched(arg(e,0));auto rect=resolve(arg(e,1));
            auto begin=controls.size();imageControls(content,{},name,depth+1);
            float minX=0,minY=0,maxX=0,maxY=0;
            if(e->name=="AnimatedImagePaneRect"){minX=number(arg(rect,0));minY=-number(arg(rect,3));maxX=number(arg(rect,2));maxY=-number(arg(rect,1));}
            else {auto b=controlsBounds(begin,true);minX=b.x;minY=b.y;maxX=b.x+b.z;maxY=b.y+b.w;}
            extent={maxX-minX,maxY-minY};
            for(size_t i=begin;i<controls.size();++i)controls[i].translate(origin-glm::vec2(minX,minY));
            return;
        }
        if(e->name=="EditPane" || e->name=="StringComboPane" || e->name=="StringColorComboPane") {
            bool edit=e->name=="EditPane";auto face=point(arg(e,0)),size=edit?face:glm::max(face,point(arg(e,1)));
            OriginalControl c{OriginalControl::Kind::Slot,{},{},{},{0,0,size.x,size.y}};c.faceSize=face;
            c.widget=edit?OriginalControl::Widget::Edit:e->name=="StringColorComboPane"?OriginalControl::Widget::ColorCombo:OriginalControl::Widget::StringCombo;
            if(!edit){c.hoverSound=mouseoverSound;c.selectSound=mouseclickSound;}
            font(c,arg(e,edit?1:2));if(!edit)c.color=color(arg(e,3));add(std::move(c));return;
        }
        if(e->name=="ListPane" || e->name=="ListWithScrollBarPane" || e->name=="ScrollBarPane" || e->name=="PagePane") {
            auto size=point(arg(e,0));OriginalControl c;c.kind=OriginalControl::Kind::Slot;c.bounds={0,0,size.x,size.y};
            c.widget=e->name=="ScrollBarPane"?OriginalControl::Widget::ScrollBar:e->name=="PagePane"?OriginalControl::Widget::Page:OriginalControl::Widget::List;
            auto bar=arg(e,1);if(bar && bar->kind==E::Kind::Reference)c.scrollBar=controlName(bar->name);add(std::move(c));return;
        }
        if(e->name=="ChatListPane") {
            OriginalControl c;c.kind=OriginalControl::Kind::Slot;c.widget=OriginalControl::Widget::Chat;
            c.faceSize={number(arg(e,0)),number(arg(e,1))};c.chatFont=int(number(arg(e,2)));c.showScrollBar=number(arg(e,3))!=0;
            float bar=c.showScrollBar?assets.image("scrollupbmp",true)->width:0;
            c.bounds={0,0,c.faceSize.x+bar,c.faceSize.y};add(std::move(c));return;
        }
    }
};
}
const UiScript& OriginalUi::linkedScript(const std::string& name) {
    if(auto it=scripts_.find(name);it!=scripts_.end())return it->second;
    auto source=UiScript::read(assets_.find(name+".mdl"));UiScript linked;linked.imports=source.imports;
    for(auto& imported:source.imports) {
        std::filesystem::path path;try{path=assets_.find(imported+".mdl");}catch(const std::exception&){continue;} // Host-provided namespace.
        std::ifstream file(path,std::ios::binary);char first{};file.get(first);if(!file || !first)continue; // Binary font/art namespace.
        const auto& dependency=linkedScript(imported);
        for(auto& [key,value]:dependency.definitions)linked.definitions.try_emplace(key,value);
    }
    // Qualify static references in their defining namespace. Imported panels
    // each define width/color/position variables; flattening those names would
    // silently substitute the importing screen's unrelated values.
    std::function<P(P)> qualify=[&](P e)->P {
        auto copy=std::make_shared<E>(*e);
        if(e->kind==E::Kind::Reference) {
            if(source.definitions.contains(e->name))copy->name=name+"::"+e->name;
            else if(auto it=linked.definitions.find(e->name);it!=linked.definitions.end())return it->second;
        }
        for(auto& child:copy->arguments)child=qualify(child);return copy;
    };
    for(auto& [key,value]:source.definitions)linked.definitions[name+"::"+key]=qualify(value);
    for(auto& [key,value]:source.definitions)linked.definitions[key]=std::make_shared<E>(E{E::Kind::Reference,name+"::"+key});
    return scripts_.emplace(name,std::move(linked)).first->second;
}
std::vector<OriginalControl> OriginalUi::layout(const std::string& name,const std::string& root,const UiNumbers& numbers,const UiStrings& strings,bool colorKeyHint,const UiSlots& slots) {
    const auto& script=linkedScript(name);auto definition=script.definitions.find(root);if(definition==script.definitions.end())throw std::runtime_error("Original UI root missing: "+name+" / "+root);
    Compiler compiler{assets_,script,numbers,strings,{},slots.empty()?nullptr:&slots,colorKeyHint};compiler.pane(definition->second,{},root);return std::move(compiler.controls);
}
glm::vec2 OriginalUi::paneSize(const std::string& name,const std::string& root) {
    const auto& script=linkedScript(name);UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};return compiler.pane(script.definitions.at(root),{},root);
}
std::string OriginalUi::string(const std::string& name,const std::string& key) {
    const auto& script=linkedScript(name);auto definition=script.definitions.find(key);
    if(definition==script.definitions.end())return {};
    UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};
    return compiler.string(definition->second);
}
double OriginalUi::number(const std::string& name,const std::string& key,const UiNumbers& numbers) {
    const auto& script=linkedScript(name);auto definition=script.definitions.find(key);
    if(definition==script.definitions.end())throw std::runtime_error("Original UI number missing: "+name+" / "+key);
    UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};return compiler.number(definition->second);
}
glm::vec2 OriginalUi::point(const std::string& name,const std::string& key) {
    const auto& script=linkedScript(name);UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};
    return compiler.point(script.definitions.at(key));
}
OriginalControl OriginalUi::font(const std::string& name,const std::string& key,int argument) {
    const auto& script=linkedScript(name);UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};
    OriginalControl result;result.kind=OriginalControl::Kind::Text;auto found=script.definitions.find(key);
    auto source=found!=script.definitions.end()?found->second:std::make_shared<E>(E{E::Kind::Reference,key});compiler.font(result,argument<0?source:compiler.arg(source,size_t(argument)));return result;
}
glm::vec4 OriginalUi::color(const std::string& name,const std::string& key) {
    const auto& script=linkedScript(name);UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};
    if(auto found=script.definitions.find(key);found!=script.definitions.end())return compiler.color(found->second);
    if(auto value=originalColor(key))return *value;throw std::runtime_error("Original UI color missing: "+name+" / "+key);
}
std::vector<int> OriginalUi::integers(const std::string& name,const std::string& key,const UiNumbers& numbers) {
    const auto& script=linkedScript(name);auto definition=script.definitions.find(key);
    if(definition==script.definitions.end())throw std::runtime_error("Original UI integer list missing: "+name+" / "+key);
    UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};auto list=compiler.resolve(definition->second);
    if(!list || list->kind!=E::Kind::List)throw std::runtime_error("Original UI integer list required: "+name+" / "+key);
    std::vector<int> result;result.reserve(list->arguments.size());
    for(auto& item:list->arguments)result.push_back(int(compiler.number(item)));
    return result;
}
std::vector<OriginalChoice> OriginalUi::choices(const std::string& name,const std::string& names,std::initializer_list<const char*> tables) {
    const auto& script=linkedScript(name);UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};
    auto list=compiler.resolve(script.definitions.at(names));std::vector<OriginalChoice> result;
    for(auto& entry:list->arguments)result.push_back({compiler.string(entry),{}});
    for(auto table:tables){auto values=compiler.resolve(script.definitions.at(table));
        if(values->arguments.size()!=result.size())throw std::runtime_error("Original combo table size mismatch: "+name+" / "+table);
        for(size_t i=0;i<result.size();++i)result[i].values.push_back(compiler.number(values->arguments[i]));}
    return result;
}
std::vector<OriginalLoadoutSlot> OriginalUi::loadoutSlots(int hull) {
    const auto& script=linkedScript("loadout");
    auto it=script.definitions.find("loadout"+std::to_string(hull));
    if(it==script.definitions.end())it=script.definitions.find("loadoutDefault");
    if(it==script.definitions.end())return {};
    UiNumbers types{{"equipmentTypeCargo",-1},{"equipmentTypeTurret",-2},{"equipmentTypeChaff",0},{"equipmentTypeWeapon",1},
        {"equipmentTypeMagazine",2},{"equipmentTypeDispenser",3},{"equipmentTypeShield",4},{"equipmentTypeCloak",5},{"equipmentTypePack",6},{"equipmentTypeAfterburner",7}};
    UiStrings strings;Compiler compiler{assets_,script,types,strings,{}};std::vector<OriginalLoadoutSlot> result;
    auto list=compiler.resolve(it->second);if(!list)return result;
    for(auto& entry:list->arguments)result.push_back({int(compiler.number(compiler.arg(entry,0))),int(compiler.number(compiler.arg(entry,1))),
        compiler.string(compiler.arg(entry,2)),compiler.point(compiler.arg(entry,4)),compiler.image(compiler.arg(entry,3))});
    return result;
}
std::shared_ptr<ImageData> OriginalUi::loadoutImage(const OriginalLoadoutSlot& slot,const std::string& model) {
    auto name=slot.prefix+model+"bmp";
    if(!assets_.hasImage(name))name="l"+model+"bmp";
    return assets_.image(name,true);
}
std::vector<OriginalControl> OriginalUi::loadoutOverlay(int hull) {
    const auto& script=linkedScript("loadout");auto name="loadout"+std::to_string(hull)+"overlay";
    auto it=script.definitions.find(name);if(it==script.definitions.end())return {};
    UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};
    auto position=point("loadoutpane","geoPosition"),size=point("loadoutpane","geoSize");
    compiler.imageControls(it->second,position+glm::vec2(0,size.y),name,0);
    for(auto& c:compiler.controls)c.intersectClip({position,size});return std::move(compiler.controls);
}
std::vector<OriginalControl> OriginalUi::imageLayout(const std::string& name,const std::string& root,const UiNumbers& numbers,const UiStrings& strings,const UiSlots& slots) {
    const auto& script=linkedScript(name);auto it=script.definitions.find(root);if(it==script.definitions.end())throw std::runtime_error("Original image root missing: "+name+" / "+root);
    Compiler compiler{assets_,script,numbers,strings,{},&slots};compiler.imageControls(it->second,{},root,0);return std::move(compiler.controls);
}
std::vector<OriginalImageItem> OriginalUi::imageList(const std::string& name,const std::string& root,const UiNumbers& numbers) {
    const auto& script=linkedScript(name);UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};
    auto list=compiler.resolve(script.definitions.at(root));
    if(!list || list->kind!=E::Kind::List)throw std::runtime_error("Original image list expected: "+name+" / "+root);
    std::vector<OriginalImageItem> result;result.reserve(list->arguments.size());
    for(auto row:list->arguments){
        row=compiler.resolve(row);
        if(!row || row->kind!=E::Kind::Tuple || row->arguments.size()!=2)throw std::runtime_error("Original image/value pair expected");
        auto value=compiler.resolve(row->arguments[1]);
        auto negative=value && value->kind==E::Kind::Operator && value->name=="negate" && value->arguments.size()==1?compiler.resolve(value->arguments[0]):P{};
        if(!value || (value->kind!=E::Kind::Number && !(negative && negative->kind==E::Kind::Number) && !(value->kind==E::Kind::Reference && numbers.contains(value->name))))
            throw std::runtime_error("Original image list value missing: "+(value?value->name:std::string{}));
        compiler.imageControls(row->arguments[0],{},root,0);
        result.push_back({std::move(compiler.controls),compiler.number(value)});compiler.controls.clear();
    }
    return result;
}
glm::vec2 OriginalUi::imageSize(const std::string& name,const std::string& root) {
    const auto& script=linkedScript(name);auto it=script.definitions.find(root);if(it==script.definitions.end())return {};
    UiNumbers numbers;UiStrings strings;Compiler compiler{assets_,script,numbers,strings,{}};auto b=compiler.imageBounds(it->second);return {b.z,b.w};
}
}
