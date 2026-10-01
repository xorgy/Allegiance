#include "hud_program.hpp"
#include "ui_constants.hpp"
#include "../ClientCore/AuthoredText.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>

namespace Native {
namespace {
using E=UiExpression;using P=std::shared_ptr<const E>;
std::string leaf(std::string name){if(auto p=name.rfind("::");p!=name.npos)name.erase(0,p+2);return name;}
}
struct HudProgram::Impl {
    Assets& assets;const UiScript* script;int style;bool paneColorKey{};
    enum class Op {Constant,Input,Add,Subtract,Multiply,Divide,Mod,Min,Max,Equal,NotEqual,And,Or,BitAnd,BitOr,Not,Color,Guard};
    struct Number {Op op;int a{},b{};float value{};};
    struct Piece {enum class Kind {Literal,Input,Number,RealNumber};Kind kind;std::string literal;int value{},guard{};};
    struct Instruction {
        HudDraw draw;int x{},y{},guard{},fraction{},width{-1},height{-1};glm::vec4 crop{};
        int gauge{-1};bool gauge3d{true},pane{};std::array<int,4> color{};
        std::vector<Piece> text;
        bool measured{};std::shared_ptr<const BitmapFont> metrics;std::string measuredText;float measuredHeight{};
    };
    struct Layout {
        enum Kind {FixedPane,Normalize,PaneClip,FromPane,Black,Clip,Justify,Tint,UnionPane} kind;
        size_t begin{},end{};int x{},y{},surface{-1},justify{};glm::vec2 size{};std::array<int,4> rectangle{};
    };
    std::vector<Number> numbers;std::vector<float> results;std::vector<HudFont> fonts;
    std::vector<Instruction> instructions;std::vector<HudDraw> output;
    std::vector<Layout> layouts;std::vector<glm::vec4> geometry,surfaces;std::vector<size_t> drawIndices;int rootPane{-1};
    std::vector<std::shared_ptr<ImageData>> images;
    std::unordered_map<const E*,int> numericCache;
    std::unordered_map<std::string,int> constants;
    struct Overlay {std::array<int,2> off,on;int side;};
    std::vector<Overlay> overlays;
    std::vector<HudOverlay> overlayOutput;
    glm::vec2 paneExtent{};int scalarOutput{-1};
    Impl(OriginalUi& ui,std::string_view root,int s,std::string_view source,bool panes):Impl(ui.assets(),ui.script(std::string(source)),root,s,panes){}
    Impl(Assets& a,const UiScript& linked,std::string_view root,int s,bool panes,std::span<const std::string_view> overlayRoots={},bool scalar=false):assets(a),script(&linked),style(s),paneColorKey(true) {
        constants={{"StyleHUD",s},{"JustifyLeft",0},{"JustifyCenter",1},{"JustifyRight",2},
            {"JustifyTop",4},{"JustifyBottom",8},{"true",1},{"false",0},
            {"SideLeft",0},{"SideTopLeft",1},{"SideTop",2},{"SideTopRight",3},{"SideRight",4},
            {"SideBottomRight",5},{"SideBottom",6},{"SideBottomLeft",7},{"SideCenter",8}};
        int zero=constant(0),one=constant(1);
        auto found=script->definitions.find(root.empty()?"dialogs":std::string(root));if(found==script->definitions.end())throw std::runtime_error("MDL root missing: "+std::string(root));
        if(scalar)scalarOutput=number(found->second);
        else if(root.empty())compileOverlays(found->second,overlayRoots);
        else if(panes) {
            rootPane=pane(found->second,zero,zero,one,false,0);
        }else image(found->second,zero,zero,one,false,0);
        results.resize(numbers.size());output.resize(instructions.size());
        geometry.resize(instructions.size());drawIndices.resize(instructions.size());
        for(auto& item:output)item.text.reserve(512);
        for(auto& i:instructions)if(i.measured){auto& f=fonts[i.draw.font];i.metrics=assets.font(f.symbol,f.height,f.bold);i.measuredText.reserve(512);}
        for(auto& i:instructions)if(i.draw.image && std::find(images.begin(),images.end(),i.draw.image)==images.end())images.push_back(i.draw.image);
        // Compilation is complete. Runtime records never retain namespace or
        // AST references, including after the caller releases OriginalUi.
        script=nullptr;numericCache.clear();numericCache.rehash(0);constants.clear();constants.rehash(0);
        if(rootPane>=0)evaluate(HudValues{});
    }
    P resolve(P p)const {
        for(unsigned n=0;p && p->kind==E::Kind::Reference;++n){if(n>128)throw std::runtime_error("Cyclic HUD expression");
            auto i=script->definitions.find(p->name);if(i==script->definitions.end())break;p=i->second;}
        return p;
    }
    P arg(P p,size_t n)const {p=resolve(p);return p && n<p->arguments.size()?p->arguments[n]:P{};}
    void compileOverlays(P source,std::span<const std::string_view> roots) {
        auto list=resolve(source);if(!list || list->kind!=E::Kind::List)throw std::runtime_error("MDL dialogs must be a list");
        for(auto entry:list->arguments) {
            entry=resolve(entry);if(!entry || entry->arguments.size()!=7)throw std::runtime_error("MDL overlay requires seven fields");
            auto root=arg(entry,0);HudOverlay out;
            if(root && root->kind==E::Kind::Reference)out.root=leaf(root->name);if(!roots.empty() && std::find(roots.begin(),roots.end(),out.root)==roots.end())continue;
            int side=int(staticNumber(arg(entry,1)));if(side<0 || side>8)throw std::runtime_error("Invalid MDL overlay screen anchor");
            constexpr glm::vec2 anchors[]={{0,.5f},{0,0},{.5f,0},{1,0},{1,.5f},{1,1},{.5f,1},{0,1},{.5f,.5f}};
            out.anchor=anchors[side];
            auto point=[&](P p){p=resolve(p);if(!p || p->name!="Point" || p->arguments.size()!=2)throw std::runtime_error("MDL overlay requires a point");return std::array<int,2>{number(arg(p,0)),number(arg(p,1))};};
            Overlay compiled{point(arg(entry,2)),point(arg(entry,3)),side};
            out.duration=staticNumber(arg(entry,4));out.mask=uint32_t(staticNumber(arg(entry,5)));out.undetectable=staticNumber(arg(entry,6))!=0;
            if(out.duration<0)throw std::runtime_error("Negative MDL overlay transition duration");
            overlays.push_back(compiled);overlayOutput.push_back(std::move(out));
        }
    }
    std::string key(P p)const {
        if(!p)return {};auto name=leaf(p->name);
        if(p->kind==E::Kind::Call && !p->arguments.empty())name+='.'+leaf(p->arguments[0]->name);
        return name;
    }
    static const std::unordered_map<std::string,HudNumber>& bindings(){
        static const std::unordered_map<std::string,HudNumber> table={
            {"ShowCenterHUD",HudNumber::ShowCenter},{"ShowTargetHUD",HudNumber::ShowTarget},
            {"GetPercentHitPoints.Me",HudNumber::Hull},{"GetPercentShields.Me",HudNumber::Shield},
            {"GetPercentEnergy.Me",HudNumber::Energy},{"GetFuel.Me",HudNumber::Fuel},{"GetAmmo.Me",HudNumber::Ammo},
            {"playerThrottle",HudNumber::Throttle},{"GetSpeed.Me",HudNumber::Speed},{"Experience",HudNumber::Experience},
            {"Signature",HudNumber::Signature},{"Flash",HudNumber::Flash},{"GotArtifact",HudNumber::Artifact},{"GotFlag",HudNumber::Flag},
            {"Detected",HudNumber::Detected},{"GetVectorLock.Me",HudNumber::VectorLock},{"AutopilotOn",HudNumber::Autopilot},
            {"IsCloaked.Me",HudNumber::Cloak},{"IsEjectPod.Me",HudNumber::Pod},{"GetEndurance.Me",HudNumber::Endurance},
            {"GetCloaking.Me",HudNumber::Cloaking},
            {"IsRipcording.Me",HudNumber::Ripcord},{"GetRipcordTimeLeft.Me",HudNumber::RipcordLeft},
            {"GetPercentHitPoints.Target",HudNumber::TargetHull},{"GetPercentShields.Target",HudNumber::TargetShield},
            {"GetSpeed.Target",HudNumber::TargetSpeed},{"GetRange.Target",HudNumber::TargetRange},{"TargetExperience",HudNumber::TargetExperience},
            {"GetModelType.Target",HudNumber::TargetType},{"IsNotNull.Target",HudNumber::TargetExists},{"TargetInRangeFlag",HudNumber::InRange},
            {"GetPercentHitPoints.MyStation",HudNumber::StationHull},{"GetPercentShields.MyStation",HudNumber::StationShield},
            {"Lives",HudNumber::Lives},{"LivesOn",HudNumber::LivesOn},{"Kills",HudNumber::Kills},{"KillsOn",HudNumber::KillsOn},
            {"CountdownOn",HudNumber::CountdownOn},{"BoardState",HudNumber::BoardState},{"IsGhost",HudNumber::Ghost},{"NumChatLines",HudNumber::ChatLines},{"creditsTime",HudNumber::UiTime},
            {"GetWeaponRange.part",HudNumber::PartRange},{"GetDamage.part",HudNumber::PartDamage},{"GetRate.part",HudNumber::PartRate},
            {"GetCount.part",HudNumber::PartCount},{"GetReadyState.part",HudNumber::PartReady},{"GetMountedFraction.part",HudNumber::PartMounted},
            {"GetArmedFraction.part",HudNumber::PartArmed},{"IsSelected.part",HudNumber::PartSelected},{"IsActive.part",HudNumber::PartActive},
            {"IsOutOfAmmo.part",HudNumber::PartEmpty},{"GetAfterburnerTopSpeed.part",HudNumber::PartAfterburnSpeed},
            {"GetAfterburnerTimeLeft.part",HudNumber::PartAfterburnTime},{"GetShieldStrength.part",HudNumber::PartShieldStrength},
            {"GetRechargeTime.part",HudNumber::PartRechargeTime},{"GetCloakTimeLeft.part",HudNumber::PartCloakTime},
            {"GetNumObservers.Me",HudNumber::Observers}};
        return table;
    }
    int node(Op op,int a=0,int b=0,float value=0){numbers.push_back({op,a,b,value});return int(numbers.size())-1;}
    int constant(float f){return node(Op::Constant,0,0,f);}
    float staticNumber(P p) {int n=number(p);std::vector<float> saved=results;results.resize(numbers.size());eval({});float v=results[n];results=std::move(saved);return v;}
    bool fixed(int i)const {
        auto n=numbers[i];if(n.op==Op::Input || n.op==Op::Color)return false;if(n.op==Op::Constant)return true;
        return fixed(n.a) && (n.op==Op::Not || fixed(n.b));
    }
    int number(P p) {
        if(!p)return constant(0);if(auto i=numericCache.find(p.get());i!=numericCache.end())return i->second;
        int result;
        auto k=key(p);
        if(auto i=constants.find(k);i!=constants.end())result=constant(float(i->second));
        else if(auto i=bindings().find(k);i!=bindings().end())result=node(Op::Input,int(i->second));
        else if(p->kind==E::Kind::Number)result=constant(float(p->number));
        else if(auto r=resolve(p);r!=p)result=number(r);
        else {
            auto name=leaf(p->name);Op op;
            if(name=="PointX" || name=="PointY") {
                auto size=resolve(arg(p,0));if(size && size->name=="ImageSize") {
                    result=imageDimension(arg(size,0),name=="PointY");numericCache.emplace(p.get(),result);return result;
                }
            }
            if(name=="Add" || name=="+")op=Op::Add;else if(name=="Subtract" || name=="-")op=Op::Subtract;
            else if(name=="Multiply" || name=="*")op=Op::Multiply;else if(name=="Divide" || name=="/")op=Op::Divide;
            else if(name=="Mod")op=Op::Mod;
            else if(name=="Min")op=Op::Min;else if(name=="Max")op=Op::Max;
            else if(name=="Equals" || name=="==")op=Op::Equal;else if(name=="!=")op=Op::NotEqual;
            else if(name=="And")op=Op::And;else if(name=="Or")op=Op::Or;
            else if(name=="&")op=Op::BitAnd;else if(name=="|")op=Op::BitOr;
            else if(name=="Not"){result=node(Op::Not,number(arg(p,0)));numericCache.emplace(p.get(),result);return result;}
            else if(name=="negate"){result=node(Op::Subtract,constant(0),number(arg(p,0)));numericCache.emplace(p.get(),result);return result;}
            else throw std::runtime_error("Unsupported HUD scalar: "+k);
            int a=number(arg(p,0)),b=number(arg(p,1));result=node(op,a,b);
        }
        numericCache.emplace(p.get(),result);return result;
    }
    void eval(const HudValues& v) {
        for(size_t i=0;i<numbers.size();++i){auto n=numbers[i];float a=0,b=0,r=0;
            if(n.op!=Op::Constant && n.op!=Op::Input && n.op!=Op::Color){a=results[n.a];if(n.op!=Op::Not)b=results[n.b];}
            switch(n.op){
            case Op::Constant:r=n.value;break;case Op::Input:r=v.numbers[n.a];break;
            case Op::Add:r=a+b;break;case Op::Subtract:r=a-b;break;case Op::Multiply:r=a*b;break;case Op::Divide:r=b?a/b:0;break;
            case Op::Mod:r=b?std::fmod(a,b):0;break;
            case Op::Min:r=std::min(a,b);break;case Op::Max:r=std::max(a,b);break;case Op::Equal:r=a==b;break;case Op::NotEqual:r=a!=b;break;
            case Op::And:r=a && b;break;case Op::Or:r=a || b;break;case Op::Not:r=!a;break;case Op::Color:r=v.colors[n.a][n.b];break;
            case Op::BitAnd:r=float(uint32_t(a)&uint32_t(b));break;case Op::BitOr:r=float(uint32_t(a)|uint32_t(b));break;
            case Op::Guard:r=a?b:n.value;break;}
            results[i]=std::isfinite(r)?r:0;
        }
    }
    std::array<int,4> color(P p) {
        auto k=key(p);int binding=-1;
        if(k=="hudColor")binding=int(HudColor::Player);else if(k=="targetHudColor")binding=int(HudColor::Target);
        else if(k=="hudColorshadows")binding=int(HudColor::Shadow);else if(k=="GetSideColor.Me")binding=int(HudColor::Side);
        else if(k=="GetSideColor.Target")binding=int(HudColor::TargetSide);
        else if(k=="GetSideColor.MyStation")binding=int(HudColor::Station);
        if(binding>=0)return {node(Op::Color,binding,0),node(Op::Color,binding,1),node(Op::Color,binding,2),node(Op::Color,binding,3)};
        if(auto r=resolve(p);r!=p)return color(r);
        if(p && p->name=="Color")return {number(arg(p,0)),number(arg(p,1)),number(arg(p,2)),p->arguments.size()>3?number(arg(p,3)):constant(1)};
        if(p && p->kind==E::Kind::Reference)if(auto value=originalColor(leaf(p->name)))return {constant(value->r),constant(value->g),constant(value->b),constant(value->a)};
        throw std::runtime_error("Unsupported HUD color: "+k);
    }
    std::array<int,4> white(){return {constant(1),constant(1),constant(1),constant(1)};}
    std::string literal(P p)const {p=resolve(p);if(p && p->kind==E::Kind::String)return p->name;throw std::runtime_error("Expected HUD literal");}
    size_t font(P p) {
        auto r=resolve(p);HudFont f;
        if(r && r->name=="Win32Font"){f.symbol=literal(arg(r,0));f.height=int(staticNumber(arg(r,1)));f.bold=staticNumber(arg(r,2))!=0;}
        else if(r && r->kind==E::Kind::Reference)f.symbol=leaf(r->name);
        else throw std::runtime_error("Unsupported HUD font");
        auto i=std::find(fonts.begin(),fonts.end(),f);if(i!=fonts.end())return size_t(i-fonts.begin());fonts.push_back(std::move(f));return fonts.size()-1;
    }
    void text(P p,std::vector<Piece>& out,int guard) {
        auto k=key(p);static const std::unordered_map<std::string,HudString> table={
            {"GetModelTypeDesc.Me",HudString::Ship},{"GetName.Me",HudString::Player},{"GetName.Target",HudString::Target},
            {"GetModelTypeDesc.Target",HudString::TargetType},{"GetSectorName.Target",HudString::TargetSector},
            {"GetName.MyStation",HudString::Station},{"GetSectorName.MyStation",HudString::StationSector},{"SectorName",HudString::Sector},
            {"GetPartName.part",HudString::Part},{"partkey",HudString::PartKey},{"partlocation",HudString::PartLocation},{"GetName.gunner",HudString::Gunner},
            {"QueuedOrder",HudString::QueuedOrder},{"AcceptedOrder",HudString::AcceptedOrder},
            {"CriticalTipText0",HudString::Critical0},{"CriticalTipText1",HudString::Critical1},{"CriticalTipText2",HudString::Critical2},{"CriticalTipText3",HudString::Critical3},
            {"TipText",HudString::Tip0},{"TipText1",HudString::Tip1},{"TipText2",HudString::Tip2},{"CurrentVote",HudString::Vote}};
        if(auto i=table.find(k);i!=table.end()){out.push_back({Piece::Kind::Input,{},int(i->second),guard});return;}
        p=resolve(p);if(!p)return;
        if(p->kind==E::Kind::String){out.push_back({Piece::Kind::Literal,p->name,0,guard});return;}
        if(p->name=="ConcatinatedString" || p->name=="Concat" || p->name=="+"){text(arg(p,0),out,guard);text(arg(p,1),out,guard);return;}
        if(p->name=="NumberString"){out.push_back({Piece::Kind::Number,{},number(arg(p,0)),guard});return;}
        if(p->name=="RealNumberString"){out.push_back({Piece::Kind::RealNumber,{},number(arg(p,0)),guard});return;}
        if(p->name=="SwitchString"){switches(p,guard,[&](P branch,int g){text(branch,out,g);});return;}
        throw std::runtime_error("Unsupported HUD string: "+k);
    }
    template<class F>void switches(P p,int guard,F f,bool pane=false) {
        int value=number(arg(p,0)),remaining=guard;auto list=resolve(arg(p,pane?1:2));
        if(fixed(value)) {
            results.resize(numbers.size());eval({});float selected=results[value];
            if(list)for(auto pair:list->arguments)if(staticNumber(arg(pair,pane?1:0))==selected){f(arg(pair,pane?0:1),guard);return;}
            if(!pane)f(arg(p,1),guard);return;
        }
        if(list)for(auto pair:list->arguments){int match=node(Op::Equal,value,number(arg(pair,pane?1:0)));
            int selected=node(Op::And,guard,match);f(arg(pair,pane?0:1),selected);remaining=node(Op::And,remaining,node(Op::Not,match));}
        if(!pane)f(arg(p,1),remaining);
    }
    std::shared_ptr<ImageData> imported(P p) {
        p=resolve(p);if(!p || !(p->name=="ImportImage" || p->name=="ImportImageFromFile" || p->name=="ImportImage3D"))throw std::runtime_error("HUD gauge requires an imported image");
        auto image=assets.image(literal(arg(p,0)),paneColorKey || staticNumber(arg(p,p->name=="ImportImage3D"?2:1))!=0);image->mipmaps=false;return image;
    }
    int imageDimension(P p,bool vertical) {
        p=resolve(p);if(!p)throw std::runtime_error("ImageSize requires an image");
        if(p->name=="MDLFileImage"){OriginalUi ui(assets);auto size=ui.imageSize(literal(arg(p,0)));return constant(vertical?size.y:size.x);}
        if(p->name=="InventoryImage")return node(Op::Input,int(vertical?HudNumber::InventoryHeight:HudNumber::InventoryWidth));
        if(p->name=="SwitchImage") {
            auto value=arg(p,0);int index=number(value);if(!fixed(index))throw std::runtime_error("Dynamic ImageSize switch requires a host size binding");
            float selected=staticNumber(value);auto list=resolve(arg(p,2));
            if(list)for(auto pair:list->arguments)if(staticNumber(arg(pair,0))==selected)return imageDimension(arg(pair,1),vertical);
            return imageDimension(arg(p,1),vertical);
        }
        auto image=imported(p);return constant(float(vertical?image->height:image->width));
    }
    int surface(){surfaces.emplace_back(0);return int(surfaces.size()-1);}
    void layout(Layout value){
        if(value.kind==Layout::Normalize || value.kind==Layout::Justify || value.kind==Layout::Tint || value.kind==Layout::UnionPane)
            for(size_t n=value.begin;n<value.end;++n)if(instructions[n].draw.kind==HudDraw::Kind::Text && !instructions[n].draw.singleLine)instructions[n].measured=true;
        layouts.push_back(std::move(value));
    }
    int justification(P p){
        p=resolve(p);if(!p)return 0;auto name=leaf(p->name);
        if(name=="JustifyLeft")return 1;if(name=="JustifyRight")return 2;if(name=="JustifyTop")return 4;if(name=="JustifyBottom")return 8;
        if(name=="JustifyXCenter")return 16;if(name=="JustifyYCenter")return 32;if(name=="JustifyCenter")return 48;
        if(name=="|")return justification(arg(p,0))|justification(arg(p,1));if(name=="&")return justification(arg(p,0))&justification(arg(p,1));
        return int(staticNumber(p));
    }
    int pane(P p,int x,int y,int guard,bool additive,unsigned depth) {
        if(depth>128)throw std::runtime_error("MDL pane nesting limit");p=resolve(p);if(!p){int bounds=surface();layout({Layout::FixedPane,0,0,x,y,bounds});return bounds;}
        auto name=leaf(p->name);
        if(name=="SwitchPane"){size_t begin=instructions.size();switches(p,guard,[&](P branch,int g){pane(branch,x,y,g,additive,depth+1);},true);
            int bounds=surface();layout({Layout::UnionPane,begin,instructions.size(),x,y,bounds});return bounds;}
        if(name=="BlackPane"){
            size_t begin=instructions.size();Instruction black;black.draw.kind=HudDraw::Kind::Rectangle;black.x=x;black.y=y;black.guard=guard;black.color={constant(0),constant(0),constant(0),constant(paneColorKey?0:1)};
            instructions.push_back(std::move(black));int bounds=pane(arg(p,0),x,y,guard,additive,depth+1);layout({Layout::Black,begin,instructions.size(),x,y,bounds});return bounds;
        }
        if(name=="ImagePane") {
            // Pane coordinates are top-down; Image coordinates are bottom-up.
            size_t begin=instructions.size();image(arg(p,0),x,y,guard,additive,depth+1);
            int bounds=surface();layout({Layout::Normalize,begin,instructions.size(),x,y,bounds});
            auto children=resolve(arg(p,1));if(children)for(auto pair:children->arguments) {
                auto point=arg(pair,1);pane(arg(pair,0),node(Op::Add,x,number(arg(point,0))),node(Op::Add,y,number(arg(point,1))),guard,additive,depth+1);
            }
            layout({Layout::PaneClip,begin,instructions.size(),x,y,bounds});return bounds;
        }
        Instruction i;i.x=x;i.y=y;i.guard=guard;i.draw.additive=additive;i.color=white();i.fraction=constant(1);i.pane=true;
        if(name=="StringPane") {
            i.draw.kind=HudDraw::Kind::Text;i.draw.singleLine=true;i.draw.font=p->arguments.size()>4?font(arg(p,4)):font(std::make_shared<E>(E{E::Kind::Reference,"smallFont"}));
            auto point=arg(p,2);i.draw.bounds={0,0,staticNumber(arg(point,0)),staticNumber(arg(point,1))};
            i.draw.justify=staticNumber(arg(p,5))?3:int(staticNumber(arg(p,3)));i.color=color(arg(p,1));text(arg(p,0),i.text,constant(1));
        }else if(name=="GaugePane") {
            i.draw.kind=HudDraw::Kind::Image;i.draw.image=imported(arg(p,0));i.gauge=0;i.gauge3d=false;i.fraction=number(arg(p,1));
            i.crop={0,0,i.draw.image->width,i.draw.image->height};i.draw.bounds=i.crop;
        }else throw std::runtime_error("Unsupported compiled MDL pane: "+name);
        int bounds=surface();Layout fixed{Layout::FixedPane,instructions.size(),instructions.size()+1,x,y,bounds};fixed.size={i.draw.bounds.z,i.draw.bounds.w};layout(std::move(fixed));
        instructions.push_back(std::move(i));
        return bounds;
    }
    void image(P p,int x,int y,int guard,bool additive,unsigned depth) {
        if(depth>128)throw std::runtime_error("HUD image nesting limit");p=resolve(p);if(!p)return;
        auto name=leaf(p->name);
        if(name=="emptyImage")return;
        if(name=="SwitchImage" || name=="SwitchPane"){switches(p,guard,[&](P branch,int g){image(branch,x,y,g,additive,depth+1);},name=="SwitchPane");return;}
        if(name=="VisibleImage"){image(arg(p,0),x,y,node(Op::And,guard,number(arg(p,1))),additive,depth+1);return;}
        if(name=="TranslateImage"){auto point=arg(p,1);image(arg(p,0),node(Op::Add,x,number(arg(point,0))),node(Op::Subtract,y,number(arg(point,1))),guard,additive,depth+1);return;}
        if(name=="BlendImage"){auto mode=resolve(arg(p,1));image(arg(p,0),x,y,guard,mode && leaf(mode->name)=="BlendModeAdd",depth+1);return;}
        if(name=="GroupImage"){auto list=resolve(arg(p,0));if(list)for(auto i=list->arguments.rbegin();i!=list->arguments.rend();++i)image(*i,x,y,guard,additive,depth+1);return;}
        if(name=="JustifyImage") {size_t begin=instructions.size();image(arg(p,0),x,y,guard,additive,depth+1);Layout align{Layout::Justify,begin,instructions.size(),x,y};align.justify=justification(arg(p,1));layout(std::move(align));return;}
        if(name=="ClipImage") {size_t begin=instructions.size();image(arg(p,0),x,y,guard,additive,depth+1);auto rectangle=resolve(arg(p,1));Layout clip{Layout::Clip,begin,instructions.size(),x,y};for(int c=0;c<4;++c)clip.rectangle[c]=number(arg(rectangle,c));layout(std::move(clip));return;}
        if(name=="BlendColorImage") {
            size_t begin=instructions.size();image(arg(p,1),x,y,guard,additive,depth+1);size_t end=instructions.size();
            if(begin==end)return;int visible=constant(0);for(size_t n=begin;n<end;++n)visible=node(Op::Or,visible,instructions[n].guard);
            Instruction tint;tint.draw.kind=HudDraw::Kind::Rectangle;tint.x=x;tint.y=y;tint.guard=visible;tint.color=color(arg(p,0));instructions.push_back(std::move(tint));layout({Layout::Tint,begin,end,x,y,int(end)});return;
        }
        if(name=="PaneImage"){bool previous=paneColorKey;paneColorKey=paneColorKey || staticNumber(arg(p,2))!=0;size_t begin=instructions.size();int bounds=pane(arg(p,0),x,y,guard,additive,depth+1);paneColorKey=previous;layout({Layout::FromPane,begin,instructions.size(),x,y,bounds});return;}
        if(name=="UndetectableImage" || name=="WrapImage"){image(arg(p,0),x,y,guard,additive,depth+1);return;}
        if(name=="ImagePane"){size_t begin=instructions.size();int bounds=pane(p,x,y,guard,additive,depth+1);layout({Layout::FromPane,begin,instructions.size(),x,y,bounds});return;}
        Instruction i;i.x=x;i.y=y;i.guard=guard;i.draw.additive=additive;i.color=white();i.fraction=constant(1);
        if(name=="InventoryImage") {
            i.draw.kind=HudDraw::Kind::Inventory;i.width=node(Op::Input,int(HudNumber::InventoryWidth));i.height=node(Op::Input,int(HudNumber::InventoryHeight));
        }else if(name=="StringImage") {
            i.draw.kind=HudDraw::Kind::Text;i.draw.font=font(arg(p,2));i.draw.justify=int(staticNumber(arg(p,0)));
            i.draw.bounds.z=int(staticNumber(arg(p,1)));i.color=color(arg(p,3));text(arg(p,4),i.text,constant(1));
        }else if(name=="GeoImage") {
            i.draw.kind=HudDraw::Kind::Preview;auto rect=arg(p,1);float loX=staticNumber(arg(rect,0)),loY=staticNumber(arg(rect,1));
            i.draw.bounds={loX,-staticNumber(arg(rect,3)),staticNumber(arg(rect,2))-loX,staticNumber(arg(rect,3))-loY};
        }else if(name=="GaugeImage" || name=="GaugeImageRect") {
            auto src=resolve(arg(p,1));
            if(src && src->name=="SwitchImage") {
                switches(src,guard,[&](P branch,int selected){auto gauge=std::make_shared<E>(*p);gauge->arguments[1]=branch;image(gauge,x,y,selected,additive,depth+1);});return;
            }
            i.draw.kind=HudDraw::Kind::Image;i.draw.image=imported(arg(p,1));i.gauge=int(staticNumber(arg(p,0)));
            auto im=i.draw.image;i.crop={0,0,im->width,im->height};
            if(name=="GaugeImageRect") {auto rect=arg(p,2);for(int c=0;c<4;++c)i.crop[c]=staticNumber(arg(rect,c));i.gauge3d=staticNumber(arg(p,3))!=0;i.fraction=number(arg(p,4));}
            else {i.gauge3d=false;i.fraction=number(arg(p,2));}
            i.draw.bounds={0,-(i.crop.w-i.crop.y),i.crop.z-i.crop.x,i.crop.w-i.crop.y};
        }else if(name=="ImportImage" || name=="ImportImageFromFile" || name=="ImportImage3D") {
            i.draw.kind=HudDraw::Kind::Image;i.draw.image=imported(p);i.draw.bounds={0,-float(i.draw.image->height),i.draw.image->width,i.draw.image->height};
            if(name=="ImportImage3D")i.color=color(arg(p,1));
        }else throw std::runtime_error("Unsupported HUD image: "+name);
        instructions.push_back(std::move(i));
    }
    glm::vec4 bounds(const Layout& op)const{
        // Pane surfaces retain the origin when an authored background starts
        // to its right/below. Image justification instead uses only the image.
        glm::vec2 lo{results[op.x],results[op.y]},hi=lo;bool any=op.kind==Layout::Normalize;
        for(size_t n=op.begin;n<op.end;++n)if(drawIndices[n]!=size_t(-1)){
            const auto& box=geometry[n];const auto& draw=output[drawIndices[n]];
            // StringImage retains its declared width and zero-height baseline
            // when empty. WinTrek unions those baselines into group bounds.
            bool baseline=draw.kind==HudDraw::Kind::Text && !draw.singleLine && draw.text.empty() && box.w==0;
            if(box.z<=0 || box.w<0 || (box.w==0 && !baseline))continue;
            auto first=glm::vec2(box),last=first+glm::vec2(box.z,box.w);if(!any){lo=first;hi=last;any=true;}else{lo=glm::min(lo,first);hi=glm::max(hi,last);}
        }
        return any?glm::vec4(lo,hi-lo):glm::vec4(results[op.x],results[op.y],0,0);
    }
    static glm::vec4 intersect(glm::vec4 a,glm::vec4 b){auto lo=glm::max(glm::vec2(a),glm::vec2(b));auto hi=glm::max(lo,glm::min(glm::vec2(a)+glm::vec2(a.z,a.w),glm::vec2(b)+glm::vec2(b.z,b.w)));return {lo,hi-lo};}
    void offset(const Layout& op,glm::vec2 delta){
        for(size_t n=op.begin;n<op.end;++n)if(drawIndices[n]!=size_t(-1)){auto& d=output[drawIndices[n]];d.bounds.x+=delta.x;d.bounds.y+=delta.y;geometry[n].x+=delta.x;geometry[n].y+=delta.y;if(d.clipped){d.clip.x+=delta.x;d.clip.y+=delta.y;}}
    }
    void clip(const Layout& op,glm::vec4 rectangle){
        for(size_t n=op.begin;n<op.end;++n)if(drawIndices[n]!=size_t(-1)){auto& d=output[drawIndices[n]];d.clip=d.clipped?intersect(d.clip,rectangle):rectangle;d.clipped=true;geometry[n]=intersect(geometry[n],rectangle);}
    }
    void evaluateLayouts(){
        for(const auto& op:layouts)switch(op.kind){
        case Layout::FixedPane:surfaces[op.surface]={results[op.x],results[op.y],op.size};clip(op,surfaces[op.surface]);break;
        case Layout::Normalize:{auto box=bounds(op);offset(op,glm::vec2(results[op.x],results[op.y])-glm::vec2(box));surfaces[op.surface]={results[op.x],results[op.y],box.z,box.w};break;}
        case Layout::UnionPane:surfaces[op.surface]=bounds(op);break;
        case Layout::PaneClip:clip(op,surfaces[op.surface]);break;
        case Layout::FromPane:{glm::vec2 delta{0,-surfaces[op.surface].w};offset(op,delta);surfaces[op.surface].y+=delta.y;break;}
        case Layout::Black:if(drawIndices[op.begin]!=size_t(-1)){auto& d=output[drawIndices[op.begin]];d.bounds=geometry[op.begin]=surfaces[op.surface];}break;
        case Layout::Clip:{auto& r=op.rectangle;clip(op,{results[op.x]+results[r[0]],results[op.y]-results[r[3]],std::max(0.f,results[r[2]]-results[r[0]]),std::max(0.f,results[r[3]]-results[r[1]])});break;}
        case Layout::Justify:{auto box=bounds(op);glm::vec2 delta{};
            if(op.justify&1)delta.x=results[op.x]-box.x;else if(op.justify&2)delta.x=results[op.x]-box.x-box.z;else if(op.justify&16)delta.x=results[op.x]-box.x-box.z*.5f;
            if(op.justify&8)delta.y=results[op.y]-box.y-box.w;else if(op.justify&4)delta.y=results[op.y]-box.y;else if(op.justify&32)delta.y=results[op.y]-box.y-box.w*.5f;
            offset(op,delta);break;}
        case Layout::Tint:if(drawIndices[size_t(op.surface)]!=size_t(-1)){auto& d=output[drawIndices[size_t(op.surface)]];d.bounds=geometry[size_t(op.surface)]=bounds(op);if(d.color.a>0)for(int c=0;c<3;++c)d.color[c]/=d.color.a;}break;
        }
        if(rootPane>=0)paneExtent={surfaces[rootPane].z,surfaces[rootPane].w};
    }
    std::span<const HudDraw> evaluate(const HudValues& v) {
        eval(v);size_t count=0;
        for(size_t n=0;n<instructions.size();++n){auto& i=instructions[n];drawIndices[n]=size_t(-1);if(!results[i.guard])continue;drawIndices[n]=count;auto& d=output[count++];
            d.kind=i.draw.kind;d.bounds=i.draw.bounds;d.bounds.x+=results[i.x];d.bounds.y+=results[i.y];d.uv={0,0,1,1};d.image=i.draw.image;
            if(i.width>=0)d.bounds.z=results[i.width];if(i.height>=0){d.bounds.w=results[i.height];d.bounds.y-=d.bounds.w;}
            d.font=i.draw.font;d.justify=i.draw.justify;d.additive=i.draw.additive;d.singleLine=i.draw.singleLine;for(int c=0;c<4;++c)d.color[c]=results[i.color[c]];
            d.textBinding=i.text.size()==1 && i.text.front().kind==Piece::Kind::Input?HudString(i.text.front().value):HudString::Count;
            d.text.clear();
            for(const auto& piece:i.text)if(results[piece.guard])switch(piece.kind) {
                case Piece::Kind::Literal:d.text+=piece.literal;break;case Piece::Kind::Input:d.text+=v.strings[piece.value];break;
                case Piece::Kind::Number:{char text[48];snprintf(text,sizeof(text),"%d",int(std::clamp(results[piece.value]+.5f,-2147483520.f,2147483520.f)));d.text+=text;break;}
                case Piece::Kind::RealNumber:{char text[48];snprintf(text,sizeof(text),"%.2g",double(results[piece.value]));d.text+=text;break;}}
            geometry[n]=d.bounds;
            if(i.measured){if(i.measuredText!=d.text){i.measuredText=d.text;i.measuredHeight=i.metrics->height*Allegiance::ImageTextLines(d.text,int(d.bounds.z),[&](unsigned char ch){return i.metrics->glyphs[ch].z;});}geometry[n].w=i.measuredHeight;}
            if(i.gauge>=0) {
                auto crop=i.crop;float w=crop.z-crop.x,h=crop.w-crop.y,f=std::clamp(results[i.fraction],0.f,1.f);
                float fw=std::trunc(w*f),fh=std::trunc(h*f),ox=0,oy=0;
                if(i.gauge==0)crop.z=crop.x+fw;else if(i.gauge==2){crop.x+=w-fw;ox=w-fw;}
                else if(i.gauge==4){if(i.gauge3d)crop.y+=h-fh;else crop.w=crop.y+fh;oy=h-fh;}
                else if(i.gauge==8){if(i.gauge3d)crop.w=crop.y+fh;else crop.y+=h-fh;}
                d.bounds={results[i.x]+ox,i.pane?results[i.y]+oy:results[i.y]-oy-(crop.w-crop.y),crop.z-crop.x,crop.w-crop.y};
                float iw=float(d.image->width),ih=float(d.image->height);
                d.uv=i.gauge3d?glm::vec4(crop.x/iw,1-crop.w/ih,crop.z/iw,1-crop.y/ih):glm::vec4(crop.x/iw,crop.y/ih,crop.z/iw,crop.w/ih);
                if(d.bounds.z<0){d.bounds.x+=d.bounds.z;d.bounds.z=-d.bounds.z;std::swap(d.uv.x,d.uv.z);}
                if(d.bounds.w<0){d.bounds.y+=d.bounds.w;d.bounds.w=-d.bounds.w;std::swap(d.uv.y,d.uv.w);}
            }
            d.clipped=false;d.clip={};
        }
        evaluateLayouts();
        for(size_t n=0;n<count;++n){auto& d=output[n];if(d.clipped && (d.kind==HudDraw::Kind::Image || d.kind==HudDraw::Kind::Rectangle) && d.bounds.z>0 && d.bounds.w>0) {
                auto cropped=intersect(d.bounds,d.clip);auto lo=glm::vec2(cropped),hi=lo+glm::vec2(cropped.z,cropped.w);
                auto extent=glm::vec2(d.bounds.z,d.bounds.w),uvSize=glm::vec2(d.uv.z-d.uv.x,d.uv.w-d.uv.y),uv=glm::vec2(d.uv);
                auto first=uv+(lo-glm::vec2(d.bounds))/extent*uvSize,last=uv+(hi-glm::vec2(d.bounds))/extent*uvSize;
                d.uv={first,last};d.bounds={lo,hi-lo};
            }
        }
        return {output.data(),count};
    }
    std::span<const HudOverlay> evaluateOverlays(const HudValues& v,glm::vec2 extent) {
        eval(v);
        const glm::vec2 anchors[]={{0,extent.y*.5f},{0,0},{extent.x*.5f,0},{extent.x,0},{extent.x,extent.y*.5f},
            {extent.x,extent.y},{extent.x*.5f,extent.y},{0,extent.y},{extent.x*.5f,extent.y*.5f}};
        for(size_t i=0;i<overlays.size();++i){auto& src=overlays[i];auto& out=overlayOutput[i];auto anchor=anchors[src.side];
            out.off=anchor+glm::vec2(results[src.off[0]],-results[src.off[1]]);out.on=anchor+glm::vec2(results[src.on[0]],-results[src.on[1]]);}
        return overlayOutput;
    }
};
HudProgram::HudProgram(OriginalUi& ui,std::string_view root,int style,std::string_view script,bool pane):impl_(std::make_unique<Impl>(ui,root,style,script,pane)){}
HudProgram::HudProgram(Assets& assets,const UiScript& script,std::string_view root,int style,bool pane):impl_(std::make_unique<Impl>(assets,script,root,style,pane)){}
HudProgram::~HudProgram()=default;
HudProgram::HudProgram(HudProgram&&)noexcept=default;
HudProgram& HudProgram::operator=(HudProgram&&)noexcept=default;
std::span<const HudDraw> HudProgram::evaluate(const HudValues& v){return impl_->evaluate(v);}
std::span<const HudFont> HudProgram::fonts()const{return impl_->fonts;}
std::span<const std::shared_ptr<ImageData>> HudProgram::images()const{return impl_->images;}
size_t HudProgram::instructionCount()const{return impl_->instructions.size();}
glm::vec2 HudProgram::paneSize()const{return impl_->paneExtent;}
std::optional<OriginalControl> authoredHudControl(const HudProgram& program,const HudDraw& draw,std::string_view name,glm::vec2 origin){
    OriginalControl control;
    switch(draw.kind){
    case HudDraw::Kind::Text:{
        control.kind=OriginalControl::Kind::Text;auto& font=program.fonts()[draw.font];
        control.font=font.symbol;control.fontHeight=font.height;control.fontBold=font.bold;
        control.text=draw.text;control.singleLine=draw.singleLine;control.justify=draw.justify;
        control.textMode=draw.singleLine?OriginalControl::TextMode::Pane:OriginalControl::TextMode::Image;break;
    }
    case HudDraw::Kind::Image:control.kind=OriginalControl::Kind::Image;break;
    case HudDraw::Kind::Rectangle:control.kind=OriginalControl::Kind::Rectangle;break;
    case HudDraw::Kind::Preview:case HudDraw::Kind::Inventory:return {};
    }
    control.name=name;control.bounds=draw.bounds;control.color=draw.color;control.image=draw.image;control.uv=draw.uv;control.additive=draw.additive;
    // Image text wraps to its authored width; height zero in the VM denotes
    // no implicit surface clip. The menu painter needs a nonempty coarse bound.
    if(draw.kind==HudDraw::Kind::Text && !draw.singleLine && control.bounds.w==0)control.bounds.w=600;
    if(draw.clipped)control.clip=draw.clip;control.translate(origin);return control;
}
HudScalar::HudScalar(OriginalUi& ui,std::string_view script,std::string_view expression):impl_(std::make_unique<HudProgram::Impl>(ui.assets(),ui.script(std::string(script)),expression,0,false,std::span<const std::string_view>{},true)){}
HudScalar::~HudScalar()=default;
float HudScalar::evaluate(const HudValues& values){impl_->eval(values);return impl_->results[impl_->scalarOutput];}
HudOverlayTable::HudOverlayTable(OriginalUi& ui,int style,std::span<const std::string_view> roots):HudOverlayTable(ui.assets(),ui.script("dialog"),style,roots){}
HudOverlayTable::HudOverlayTable(Assets& assets,const UiScript& script,int style,std::span<const std::string_view> roots):impl_(std::make_unique<HudProgram::Impl>(assets,script,"",style,false,roots)){}
HudOverlayTable::~HudOverlayTable()=default;
size_t HudOverlayTable::find(std::string_view root)const {
    for(size_t i=0;i<impl_->overlayOutput.size();++i)if(impl_->overlayOutput[i].root==root)return i;
    return size_t(-1); // Removing an entry from an asset set removes its overlay.
}
std::span<const HudOverlay> HudOverlayTable::evaluate(const HudValues& values,glm::vec2 extent){return impl_->evaluateOverlays(values,extent);}
}
