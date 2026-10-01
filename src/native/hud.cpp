#include "hud.hpp"
#include "hud_program.hpp"
#include "command_input.hpp"
#include "message_pager.hpp"
#include "text_wrap.hpp"
#include "../ClientCore/AuthoredText.h"
#include "ui_clock.hpp"
#include "../ClientCore/MessageLifetime.h"
#include "../ClientCore/CommPanePresentation.h"
#include "../ClientCore/HudColors.h"
#include "training_slides.hpp"
#include "../ClientCore/Overlay.h"
#include "../ClientCore/TrainingPresentation.h"
#include "../ClientCore/StarField.h"
#include "../ClientCore/AimIndicator.h"
#include "../ClientCore/RadarPresentation.h"
#include "../ClientCore/SectorMapPresentation.h"
#include "../Igc/igc.h"
#include "system_fonts.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace Native {
namespace {
const glm::vec4 white{0.85f,0.85f,0.85f,1},muted{0.42f,0.48f,0.56f,1},green{0.12f,0.85f,0.22f,1},red{1,0.12f,0.055f,1},gold{1,0.63f,0.15f,1};
const glm::vec4 panel{0.008f,0.012f,0.022f,0.93f};
bool inside(glm::vec4 r,glm::vec2 p){return p.x>=r.x && p.y>=r.y && p.x<r.x+r.z && p.y<r.y+r.w;}
glm::vec4 authoredColor(glm::vec4 color){for(int axis=0;axis<3;++axis){float c=color[axis];color[axis]=c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}return color;}
glm::vec4 inlineTextColor(const Allegiance::InlineTextColor& code,glm::vec4 current,glm::vec4 base){
    if(!code.valid)return current;if(code.reset)return base;
    return authoredColor(glm::vec4(code.rgba[0],code.rgba[1],code.rgba[2],code.rgba[3])/255.f);
}
enum Font { Compact,CompactBold,Small,Normal,Bold,Title,FontCount };
struct Glyph {glm::vec4 uv{};float w{},h{},x{},y{},advance{};};
struct Sprite {uint32_t texture;float w,h;};
struct StringHash {
    using is_transparent=void;
    size_t operator()(std::string_view s)const{return std::hash<std::string_view>{}(s);}
};
}
struct Hud::Impl {
    Renderer& renderer;Assets& assets;float userScale,scale{1},displayScale{1};int classicScale{};bool modern{};glm::ivec2 pixels{};glm::vec2 extent{},pointer{-1,-1};
    std::array<std::array<Glyph,256>,FontCount> glyphs{};std::array<float,FontCount> ascender{};
    std::array<int,FontCount> fontPixels{};
    uint32_t fontTexture{},fontNearestTexture{};
    struct MenuAnimation {uint64_t time{};float value{};};
    std::unordered_map<const ImageData*,MenuAnimation> menuAnimations;
    std::unordered_map<std::string,Sprite,StringHash,std::equal_to<>> sprites;
    std::vector<HudQuad> backgrounds;std::vector<glm::vec2> starHistory;int starSector=-1;glm::ivec2 starSize{};
    std::vector<HudQuad> quads;std::optional<ModelPreview> preview;std::vector<uint8_t> radarVisible;
    std::vector<std::pair<std::string_view,int>> menuHoverValues;
    struct Hit {glm::vec4 bounds;HudAction action;};std::vector<Hit> hits;float hitScale{1};glm::vec4 inventoryBounds{},commandBounds{};
    bool trainingOverride{},savedInventory{},savedMap{};int savedRadar{};
    bool inventory{true},map{true},roster{},log{},diagnostics{},visible{true},cursor{},commandRadarView{};int radarMode{},cockpitRadar{},commandRadar{};
    int consoleRecipient{-1};std::array<bool,9> consoleVerbs{{true,true,true,true,true,true,true,true,true}};
    std::array<std::string,size_t(Binding::Count)> keyNames;
    std::string queuedKeys,moneyKeys,cargoKeys,groupKeys,autopilotKeys,contactsKeys,teamKeys,cursorKeys,flightKeys,historyKeys;
    std::array<std::string,size_t(CameraMode::Count)> cameraLabels;
    std::array<std::string,5> radarLabels;
    std::array<std::array<std::string,2>,16> flightHelp,interfaceHelp;
    std::array<std::string,4> helpFooters;
    std::array<int,3> lastCounters{-1,-1,-1};std::string counterText;
    struct HeaderLayout {float ghost,objective,carried,training,paused,messages;};
    HeaderLayout headers(const GameState& g)const {
        float y=g.commandView?72.f:36.f;
        auto row=[&](bool shown){float at=y;if(shown)y+=28;return at;};
        HeaderLayout result;
        result.ghost=row(visible && g.sideActive && g.ghost);result.objective=row(modern && visible && g.sideActive && !g.objective.empty());
        result.carried=row(visible && !g.carriedObjective.empty());
        result.training=row(modern && g.trainingMission && g.trainingOverlay!=NoTrainingOverlay);
        result.paused=row(g.trainingPaused);result.messages=y;return result;
    }
    bool joystickIndicator{true};VisualPreferences visual;
    MessagePager messagePager;
    int chatLines{7},chatSize{2};
    std::unique_ptr<OriginalUi> chatUi;std::vector<OriginalControl> chatControls;std::array<std::unique_ptr<HudOverlayTable>,6> chatLayouts;
    glm::vec4 chatBounds{},chatHostBounds{},chatHostClip{};float chatScale{1},chatContentWidth{};int chatGeometryKey{-1};
    Allegiance::CommPanePresentation chatPane{7,12,false};
    std::array<std::shared_ptr<const BitmapFont>,6> chatFonts;
    std::array<uint32_t,6> chatTextures{};
    struct MessageLine {uint64_t serial;double time;std::string_view text;bool warning{},continued{},wallClock{};glm::vec4 color{.85f,.85f,.85f,1};bool leader{};};
    std::vector<MessageLine> messageLines;std::vector<WrappedTextLine> wrappedLines;
    uint64_t messageUpdates{},fontGeneration{},messageFont{},messageFirst{},messageLast{},messageRevision{};size_t messageCount{};float messageScale{};
    float lastHull{1},lastShield{1};double hullFlash{},shieldFlash{};
#include "hud_authored.inl"

    Impl(Renderer& r,Assets& a,float uiScale):renderer(r),assets(a),userScale(uiScale) {
        quads.reserve(4096);messageLines.reserve(2048);wrappedLines.reserve(32);backgrounds.reserve(8192);starHistory.reserve(8192);hits.reserve(128);
        const char* names[]={
            "f104bmp","f105bmp","f106bmp","thrustgaugebmp","thrustgauge2bmp","thrustspinebmp","fuelgaugebmp","fuelgauge2bmp","fuelspinebmp",
            "ammogaugebmp","ammogauge2bmp","ammospinebmp","energygaugebmp","energygauge2bmp","energyspinebmp",
            "centerhudleftbmp","centerhudrightbmp","hullgaugebmp","hullgauge2bmp","hullgaugeredbmp",
            "shieldgaugebmp","shieldgauge2bmp","shieldgaugeredbmp","targetovalbmp","targetovalword1bmp","targetovalword2bmp",
            "targetovalcolorbmp","targetshieldgaugebmp","targethullgaugebmp","killcounterbmp","targetexperiencebmp",
            "icontechbmp","orderedbmp","radonbmp","leadinbmp","leadoutbmp","centerinbmp","centeroutbmp","directionbmp","wingbmp","concloakbmp",
            "CortUI/common/conautopilot.png","CortUI/common/convectorlock.png",
            "menuintroscreen/images/menuintroscreen_bg.jpg","CortUI/common/commandbkgnd.png"
        };
        for(auto* name:names){auto image=assets.image(name);bool flare=std::string_view(name)=="f104bmp" || std::string_view(name)=="f105bmp" || std::string_view(name)=="f106bmp";sprites.emplace(name,Sprite{renderer.addTexture(image,!flare),float(image->width),float(image->height)});}
        // Radar brackets request LoadSurface(..., true), and ConsoleImpl sets
        // the same black-key hint before loading the panel artwork. The PNGs
        // have opaque black cutouts rather than an authored alpha channel.
        const char* keyed[]={
            "tm_hud_overlaybmp","targetedbmp","lockedbmp","plockbmp","hasflagbmp","hasartifactbmp","lastfiredbmp",
            "CortUI/common/verbs.png","CortUI/common/recipients.png","CortUI/common/btnchangewing3.png","sectorripcordbmp",
            "sectoremptybmp","sectorownerbmp","sectorinvaderbmp","sectorhighlightbmp","sectorselbmp","sectortargetbmp","sectorqueuedbmp",
            "sectorripcordtargetbmp","sectorripcordstationbmp","sectorripcordstationtargetbmp",
            "sectorcapitalbmp","sectorcombatbmp","sectorwarningbmp","sectorbomberbmp","sectorminerbmp",
            "consectorinfobmp","conesectorinfobmp","consectorexpandbmp","probebmp",
            "CortUI/classic/minimappanelexpanded.png","CortUI/classic/minimapexpandbtn.png",
            "CortUI/classic/inventorytop.png","CortUI/classic/inventorylinebkgnd.png","CortUI/classic/inventorybottom.png",
            "CortUI/classic/inventoryselected.png","CortUI/classic/inventorytab.png","CortUI/classic/minimappanel.png","CortUI/classic/sectormapbg.png"
        };
        for(auto* name:keyed){auto image=assets.image(name,true);sprites.emplace(name,Sprite{renderer.addTexture(image,true),float(image->width),float(image->height)});}
        chatUi=std::make_unique<OriginalUi>(assets);auto& original=*chatUi;
        preloadAuthoredHud(original);
        auto chat=original.layout("dialog","chatListPane",{{"ChatFontSize",1},{"NumChatLines",7}}).front();chatContentWidth=chat.faceSize.x;
        constexpr std::string_view chatRoots[]={"cortCommPaneImage"};
        for(int style=0;style<6;++style)chatLayouts[style]=std::make_unique<HudOverlayTable>(original,style,chatRoots);
        for(int size=1;size<=3;++size)for(int bold=0;bold<2;++bold){auto index=size_t((size-1)*2+bold);chatFonts[index]=assets.font(Allegiance::ChatFontName(size,bold));chatTextures[index]=renderer.addTexture(chatFonts[index]->coverage,true);}
        resize(renderer.size());
        Settings defaults;for(size_t i=0;i<keyNames.size();++i)keyNames[i]=keyName(defaults,Binding(i));cacheCaptions();
    }
    void resize(glm::ivec2 size,float explicitScale=0) {
        pixels=size;
        // Edge anchors and an independent user scale; preserve circles/aspect and
        // keep all three bottom panels separate on narrow windows as well.
        scale=explicitScale>0?explicitScale:modern?interfaceScale(size,userScale):float(originalInterfaceScale(size,classicInterfaceScale(classicScale,displayScale)));scale=std::max(0.2f,scale);
        extent=glm::vec2(size)/scale;
        // Round the final font heights, not the layout scale: a separately
        // quantized scale made text grow relative to its artwork at some sizes.
        // Match the original bitmap fonts' visible height (smallFont has a
        // 7-pixel capital), rather than their line height including padding.
        constexpr int sizes[]={9,9,10,11,12,16};
        std::array<int,FontCount> requested{};
        for(int font=0;font<FontCount;++font)requested[font]=std::max(5,int(std::round(sizes[font]*(modern?std::min(scale,3.0f):1.f))));
        if(fontPixels==requested)return;fontPixels=requested;
        auto atlas=std::make_shared<ImageData>();atlas->width=2048;atlas->height=1024;atlas->mipmaps=false;atlas->name="HUD glyph coverage";
        atlas->format=ImageFormat::Coverage;atlas->pixels.resize(atlas->byteSize());
        int x=2,y=2,row=0;
        for(int font=0;font<FontCount;++font) {
            auto face=assets.systemFonts().face(font==CompactBold || font>=Bold);
            FT_Set_Pixel_Sizes(face,0,fontPixels[font]);ascender[font]=float(face->size->metrics.ascender)/64;
            for(unsigned c=32;c<256;++c) {
                if(FT_Load_Char(face,c,FT_LOAD_RENDER))continue;auto& b=face->glyph->bitmap;
                if(x+int(b.width)+2>int(atlas->width)){x=2;y+=row+2;row=0;}
                if(y+int(b.rows)+2>int(atlas->height))throw std::runtime_error("HUD font atlas exhausted");
                for(unsigned j=0;j<b.rows;++j)for(unsigned i=0;i<b.width;++i)
                    atlas->pixels[(y+j)*atlas->width+x+i]=b.buffer[j*b.pitch+i];
                glyphs[font][c]={{float(x)/atlas->width,float(y)/atlas->height,float(x+b.width)/atlas->width,float(y+b.rows)/atlas->height},
                    float(b.width),float(b.rows),float(face->glyph->bitmap_left),float(face->glyph->bitmap_top),float(face->glyph->advance.x)/64};
                x+=b.width+2;row=std::max(row,int(b.rows));
            }
        }
        // Same resident allocation/descriptor on resize. The upload is fenced by
        // the renderer's current frame; no font cache growth while resizing.
        if(fontTexture)renderer.updateTexture(fontTexture,*atlas);
        else {fontTexture=renderer.addTexture(atlas);fontNearestTexture=renderer.addTexture(atlas,true);}
        ++fontGeneration;
    }
    void rect(float x,float y,float w,float h,glm::vec4 color,uint32_t texture=0,glm::vec4 uv={0,0,1,1}) {
        if(w<=0 || h<=0)return;
        quads.push_back({{x*2/extent.x-1,y*2/extent.y-1,(x+w)*2/extent.x-1,(y+h)*2/extent.y-1},uv,color,texture});
    }
    void line(glm::vec2 a,glm::vec2 b,float width,glm::vec4 color) {
        glm::vec2 delta=b-a;float length=glm::length(delta);if(length<0.001f)return;
        glm::vec2 normal=glm::vec2(-delta.y,delta.x)/length*width/extent;
        quads.push_back({{a*2.0f/extent-glm::vec2(1),b*2.0f/extent-glm::vec2(1)},{normal,0,0},color,hudLine});
    }
    unsigned next(std::string_view value,size_t& i)const {
        unsigned c=uint8_t(value[i++]);if(c<128)return c;
        if(c>=0xc2 && c<=0xc3 && i<value.size() && (uint8_t(value[i])&0xc0)==0x80)return ((c&31)<<6)|(uint8_t(value[i++])&63);
        while(i<value.size() && (uint8_t(value[i])&0xc0)==0x80)++i;return '?';
    }
    float width(std::string_view value,Font font)const {
        float result=0;for(size_t i=0;i<value.size();){if(auto bytes=Allegiance::TextColorBytes(value.substr(i))){i+=bytes;continue;}unsigned c=next(value,i);if(c>=32)result+=glyphs[font][c].advance;}
        return result/(modern?scale:1.f);
    }
    void text(float x,float y,std::string_view value,glm::vec4 color=white,Font font=Normal,float align=0,float maxWidth=0) {
        float rasterScale=modern?scale:1.f;
        float measured=width(value,font);std::string_view suffix;
        if(maxWidth>0 && measured>maxWidth) {
            suffix="...";while(!suffix.empty() && width(suffix,font)>maxWidth)suffix.remove_suffix(1);
            float available=maxWidth-width(suffix,font);measured=0;size_t end=0;
            for(size_t i=0;i<value.size();) {
                if(auto bytes=Allegiance::TextColorBytes(value.substr(i))){i+=bytes;end=i;continue;}
                unsigned c=next(value,i);float advance=c>=32?glyphs[font][c].advance/rasterScale:0;
                if(measured+advance>available)break;measured+=advance;end=i;
            }
            value=value.substr(0,end);measured+=width(suffix,font);
        }
        x-=measured*align;
        // Snap the text origin, not each glyph advance, to device pixels. Glyphs
        // render at their rasterized size instead of stretching a fixed atlas.
        x=std::round(x*rasterScale)/rasterScale;y=std::round(y*rasterScale)/rasterScale;float origin=x;
        auto baseColor=color;
        for(size_t i=0;i<value.size();) {
            auto remaining=value.substr(i);Allegiance::InlineTextColor code;if(Allegiance::ConsumeTextColor(remaining,code)){i=value.size()-remaining.size();color=inlineTextColor(code,color,baseColor);continue;}
            unsigned c=next(value,i);if(c=='\n'){x=origin;y+=(fontPixels[font]+5)/rasterScale;continue;}if(c<32)continue;
            auto& g=glyphs[font][c];
            rect(x+g.x/rasterScale,y+(ascender[font]-g.y)/rasterScale,g.w/rasterScale,g.h/rasterScale,color,modern?fontTexture:fontNearestTexture,g.uv);x+=g.advance/rasterScale;
        }
        if(!suffix.empty())text(x,y,suffix,color,font);
    }
    void label(glm::vec4 bounds,std::string_view value,glm::vec4 color=white,Font font=Compact,float align=0) {
        // Align the visible capital height to the row, not the font's ascender
        // padding. Descenders retain the same baseline and fit in the row inset.
        float rasterScale=modern?scale:1.f;
        float y=bounds.y+(bounds.w+glyphs[font]['H'].y/rasterScale)*0.5f-ascender[font]/rasterScale;
        text(bounds.x+bounds.z*align,y,value,color,font,align,bounds.z);
    }
    void image(std::string_view name,float x,float y,glm::vec4 color=glm::vec4(1),bool additive=true) {
        if(name.starts_with("CortUI/classic/"))name=hudPanel(name);
        const auto& s=sprites.find(name)->second;rect(x,y,s.w,s.h,color,s.texture|(additive?hudAdditive:0));
    }
    void bitmapText(std::string_view name,float x,float y,std::string_view value,glm::vec4 color,float width) {
        auto font=assets.font(std::string(name));auto texture=renderer.addTexture(font->coverage,true);
        auto baseColor=color;
        while(!value.empty()){Allegiance::InlineTextColor code;if(Allegiance::ConsumeTextColor(value,code)){color=inlineTextColor(code,color,baseColor);continue;}auto ch=static_cast<unsigned char>(value.front());value.remove_prefix(1);auto g=font->glyphs[ch];if(g.z>width)break;
            rect(x,y,g.z,g.w,color,texture,{g.x/font->coverage->width,g.y/font->coverage->height,(g.x+g.z)/font->coverage->width,(g.y+g.w)/font->coverage->height});x+=g.z;width-=g.z;}
    }
    void expanded(std::string_view name,glm::vec2 center,float radius,glm::vec4 color) {
        const auto& s=sprites.find(name)->second;float w=s.w*.5f,h=s.h*.5f;
        float d=std::max(radius/std::sqrt(2.f),s.w*.25f);
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)
            rect(center.x+(x?d:-d)-w*.5f,center.y+(y?d:-d)-h*.5f,w,h,color,s.texture,{x*.5f,y*.5f,(x+1)*.5f,(y+1)*.5f});
    }
    void legacyImage(std::string_view name,glm::vec2 origin,float x,float y,glm::vec4 color=glm::vec4(1),bool additive=true) {
        const auto& s=sprites.find(name)->second;image(name,origin.x+x,origin.y-y-s.h,color,additive);
    }
    void legacyText(glm::vec2 origin,float x,float y,float w,std::string_view value,glm::vec4 color=white,Font font=Compact,float align=0) {
        text(origin.x+x+w*align,origin.y-y,value,color,font,align);
    }
    void gauge(std::string_view name,glm::vec2 origin,float x,float y,glm::vec4 crop,float fraction,bool top=false,glm::vec4 color=glm::vec4(1)) {
        // Source MDL rectangles use a bottom-left origin. Preserve both the
        // sampled subrectangle and its destination, so a gauge clips, not scales.
        const auto& s=sprites.find(name)->second;fraction=std::clamp(fraction,0.0f,1.0f);
        float w=crop.z-crop.x,h=crop.w-crop.y,offset=top?h*(1-fraction):0;float bottom=crop.y+offset,upper=bottom+h*fraction;
        rect(origin.x+x,origin.y-y-offset-h*fraction,w,h*fraction,color,s.texture|hudAdditive,{crop.x/s.w,1-upper/s.h,crop.z/s.w,1-bottom/s.h});
    }
    void button(glm::vec4 bounds,std::string_view label,HudAction action,bool selected=false,Font font=Small) {
        bool hover=cursor && inside(bounds,pointer);rect(bounds.x,bounds.y,bounds.z,bounds.w,hover?glm::vec4(0.14f,0.18f,0.23f,0.98f):panel);
        rect(bounds.x,bounds.y,bounds.z,1,selected?gold:muted);this->label({bounds.x+4,bounds.y,bounds.z-8,bounds.w},label,selected?gold:white,font,0.5f);
        hits.push_back({bounds,action});
    }
    void center(const GameState& g) {
        if(!modern)return;
        glm::vec2 o{extent.x/2-320,extent.y/2+240};char label[96];
        legacyImage("centerhudleftbmp",o,215,180);legacyImage("centerhudrightbmp",o,300,180);
        legacyImage("hullgauge2bmp",o,202,184);legacyImage("shieldgauge2bmp",o,312,185);
        gauge(g.time<hullFlash?"hullgaugeredbmp":"hullgaugebmp",o,242,207,{40,23,88,105},g.hull);
        gauge(g.time<shieldFlash?"shieldgaugeredbmp":"shieldgaugebmp",o,352,208,{40,23,88,105},g.shield);
        gauge("thrustgaugebmp",o,176,256,{15,10,110,118},1);gauge("thrustgauge2bmp",o,176,256,{15,10,110,118},g.throttle,true);legacyImage("thrustspinebmp",o,165,248);
        gauge("fuelgaugebmp",o,378,256,{17,10,113,118},1);gauge("fuelgauge2bmp",o,378,256,{17,10,113,118},g.fuel,true);legacyImage("fuelspinebmp",o,357,248);
        gauge("ammogaugebmp",o,176,123,{29,18,98,110},1);gauge("ammogauge2bmp",o,176,123,{29,18,98,110},g.ammoFraction);legacyImage("ammospinebmp",o,152,101);
        gauge("energygaugebmp",o,402,123,{30,18,99,110},1);gauge("energygauge2bmp",o,402,123,{30,18,99,110},g.energy);legacyImage("energyspinebmp",o,367,101);
        legacyText(o,205,243,100,"ARMOR");legacyText(o,340,243,100,"SHIELD",white,Compact,1);
        legacyText(o,177,381,100,"THRUST",white,Compact,1);legacyText(o,384,381,100,"FUEL");
        legacyText(o,147,108,100,"AMMO",white,Compact,1);legacyText(o,396,108,100,"ENERGY");
        snprintf(label,sizeof(label),"%.0f mps",g.speed);legacyText(o,281,152,80,label,white,Normal,0.5f);
        snprintf(label,sizeof(label),"SIG: %.0f%%",g.signature);legacyText(o,281,131,80,label,white,Compact,0.5f);
        legacyImage("killcounterbmp",o,258,37);
        snprintf(label,sizeof(label),"%.0f",g.experience);this->label({o.x+310,o.y-111,22,18},label,{0,0,0,1},CompactBold,0.5f);
        if(g.coast)legacyImage("CortUI/common/convectorlock.png",o,442,337,glm::vec4(1),false);
        if(g.autopilot)legacyImage("CortUI/common/conautopilot.png",o,177,342,glm::vec4(1),false);
        if(g.cloaked)legacyImage("concloakbmp",o,120,80);
        // Original eye is a stealth indicator: it appears while NOT detected.
        if(!g.detected)legacyImage("radonbmp",o,390,55);
        if(modern && !g.order.empty())text(extent.x/2,extent.y/2-180,g.order,gold,Small,0.5f);
        if(g.ripcord){snprintf(label,sizeof(label),"RIPCORD  %.1f",g.ripcordLeft);text(extent.x/2,extent.y/2-155,label,gold,Small,.5f);}
        if(g.pod)text(extent.x/2,extent.y/2-130,"LIFEPOD / SEEK RESCUE",gold,Small,.5f);
    }
    void queuedRequest(const GameState& g) {
        bool money=g.moneyRequest.sender>=0;if(!money && g.commands[0].command<0)return;
        std::string_view prefix=money?"REQUEST: ":"ORDER: ",label=money?(g.moneyRequest.displayPrompt.empty()?g.moneyRequest.prompt:g.moneyRequest.displayPrompt):g.commands[0].text;
        float x=extent.x*.5f-(width(prefix,Normal)+width(label,Normal))*.5f;
        text(x,extent.y*.5f-230,prefix,gold,Normal);text(x+width(prefix,Normal),extent.y*.5f-230,label,gold,Normal);
        text(extent.x*.5f,extent.y*.5f-207,money?moneyKeys:queuedKeys,muted,Small,.5f);
    }
    void targetPanel(const GameState& g) {
        if(!modern)return;
        if(g.target.empty())return;glm::vec2 o{extent.x/2-320,extent.y-25};char label[192];
        auto color=g.targetEnemy?red:green;
        legacyImage("targetovalbmp",o,192,-75);legacyImage("targetovalcolorbmp",o,256,-2,color);
        legacyImage("targetovalword1bmp",o,193,8);legacyImage("targetovalword2bmp",o,320,8);
        if(g.targetHasModel) {
            ModelPreview p;p.item=g.targetAppearance;p.item.transform=glm::scale(glm::mat4(1),glm::vec3(1/g.targetModel.radius))*g.targetOrientation;
            // CameraMotion supplies the original target-relative view, including
            // the viewing camera's roll, rather than the ship's forward alone.
            auto view=glm::translate(glm::mat4(1),glm::vec3(0,0,-2.9f))*g.targetViewOrientation;
            auto projection=glm::perspectiveRH_ZO(glm::radians(46.0f),1.0f,10.0f,0.05f);projection[1][1]*=-1;
            p.viewProjection=projection*view;p.bounds={std::round((o.x+270)*scale),std::round((o.y-105)*scale),std::round(96*scale),std::round(96*scale)};
            p.overlayOffset=quads.size();preview=p;
        }
        gauge("targethullgaugebmp",o,235,33,{44,26,83,101},g.targetHull);
        if(g.targetHasShield)gauge("targetshieldgaugebmp",o,367,33,{45,26,84,101},g.targetShield);
        legacyImage("targetexperiencebmp",o,305,89);
        snprintf(label,sizeof(label),"%.0f",g.targetExperience);this->label({o.x+310,o.y-114,22,18},label,{0,0,0,1},CompactBold,0.5f);
        snprintf(label,sizeof(label),"%s%s%s",g.target.c_str(),g.targetType.empty()?"":": ",g.targetType.c_str());this->label({o.x+160,o.y-144,320,18},label,color,Small,0.5f);
        snprintf(label,sizeof(label),"RANGE: %.0f m",g.targetDistance);legacyText(o,400,33,150,label,color);
        snprintf(label,sizeof(label),"SPEED: %.0f mps",g.targetSpeed);legacyText(o,92,33,150,label,color,Compact,1);
        legacyText(o,168,73,60,"ARMOR",white,Compact,1);legacyText(o,410,73,60,"SHIELD");
        if(g.inRange)legacyText(o,105,99,120,"IN RANGE",green,Compact,1);
        if(g.missiles) {
            snprintf(label,sizeof(label),g.missileReady<1?"RELOAD %3.0f%%":"LOCK %3.0f%%",(g.missileReady<1?g.missileReady:g.missileLock)*100);
            legacyText(o,414,99,120,label,g.missileLock>=0.99f?green:gold);
        }
    }
    void radar(const GameState& g,std::span<const Contact> contacts,const glm::mat4& vp) {
        float cx=extent.x/2,cy=extent.y/2;char label[96];
        float horizontalFov=2*std::atan(std::tan(glm::radians(g.viewFov)*.5f)*extent.x/extent.y);
        // Fixed occupancy grid: bounded stack storage and constant-time row
        // checks keep labels out of gauges, panels and other contact labels.
        std::array<uint64_t,64> occupied{};
        auto occupy=[&](glm::vec4 r,bool test) {
            if(test && (r.x<4 || r.y<4 || r.x+r.z>extent.x-4 || r.y+r.w>extent.y-4))return false;
            int x0=std::clamp(int(std::floor(r.x*64/extent.x)),0,63),x1=std::clamp(int(std::floor((r.x+r.z)*64/extent.x)),0,63);
            int y0=std::clamp(int(std::floor(r.y*64/extent.y)),0,63),y1=std::clamp(int(std::floor((r.y+r.w)*64/extent.y)),0,63);
            uint64_t mask=(~uint64_t(0)<<x0)&(~uint64_t(0)>>(63-x1));
            if(test)for(int y=y0;y<=y1;++y)if(occupied[y]&mask)return false;
            for(int y=y0;y<=y1;++y)occupied[y]|=mask;return true;
        };
        if(g.commandView) {
            auto north=vp*glm::vec4(0,5625,0,1);
            if(north.w>0){auto p=(glm::vec2(north)/north.w+1.f)*.5f*extent;occupy({p.x-width("NORTH",Bold)*.5f-3,p.y-3,width("NORTH",Bold)+6,24},false);}
            occupy({extent.x*.5f-334,40,668,30},false);
        }
        if(!g.commandView && visual.centerHud)occupy({cx-154,cy-145,308,300},false);
        if(g.commandView)occupy({extent.x-268,60,240,260},false);
        if(inventory) {
            bool cargo=modern && !g.equipment.empty() && g.equipment.back().mount<0;
            float rows=float(modern?g.equipment.size()+size_t(cargo):g.inventory.size());
            occupy({extent.x-266,extent.y-95-rows*18,246,91+rows*18},false);
        }
        if(map)occupy({40,extent.y-188,150,156},false);
        if(visual.targetHud && !g.target.empty())occupy({cx-310,extent.y-178,620,145},false);
        occupy({32,cy-4,width(counterText,Small)+8,25},false);
        occupy({32,14,width(g.sessionName,Small)+8,24},false);
        occupy({extent.x-40-width(g.sector,Bold),14,width(g.sector,Bold)+8,28},false);
        if(radarMode!=1)occupy({32,39,width(radarLabels[radarMode+1],Small)+8,24},false);
        std::string_view legend=cursor?cursorKeys:flightKeys;
        float legendWidth=width(legend,Small);occupy({cx-legendWidth*.5f-4,extent.y-24,legendWidth+8,24},false);
        auto header=headers(g);
        if(header.messages>(g.commandView?72.f:36.f))occupy({cx-344,g.commandView?70.f:34.f,688,header.messages-(g.commandView?72.f:36.f)+4},false);
        bool money=g.moneyRequest.sender>=0;
        if(modern && (money || g.commands[0].command>=0)) {
            std::string_view request=money?(g.moneyRequest.displayPrompt.empty()?g.moneyRequest.prompt:g.moneyRequest.displayPrompt):g.commands[0].text;
            float w=std::max(width(money?"REQUEST: ":"ORDER: ",Normal)+width(request,Normal),width(money?moneyKeys:queuedKeys,Small));
            occupy({cx-w*.5f-4,cy-234,w+8,53},false);
        }
        if(visual.messages) {
            if(g.camera!=CameraMode::Cockpit) {
                float w=width(cameraLabels[int(g.camera)],Small);occupy({cx-w*.5f-4,10,w+8,24},false);
            }
            size_t end=messageLines.size()-(log?std::min(messagePager.offset(),messageLines.size()):0),rows=modern?(log?MessagePager::rows:4):messagePager.pageRows();
            size_t start=end>rows?end-rows:0,count=0;double now=uiTime();
            for(size_t i=start;i<end;++i) {const auto& line=messageLines[i];if(log || Allegiance::MessageVisible(line.warning,g.trainingMission!=0,(line.wallClock?now:g.time)-line.time))++count;}
            if(count || log || !modern)occupy(modern?glm::vec4{cx-208,header.messages-2,416,float(count)*23+(log?30:2)}:glm::vec4{glm::vec2(chatHostClip)+chatAnchor(),chatHostClip.z,chatHostClip.w},false);
        }
        auto important=[&](const Contact& c){return c.selected || c.ordered || c.threat || c.objective || (g.commandView && c.commandSelected);};
        auto project=[&](const Contact& c){
            auto clip=vp*glm::vec4(c.position,1);glm::vec2 p{clip.x,clip.y};
            bool onScreen=clip.w>0 && std::abs(clip.x)<clip.w*.90f && std::abs(clip.y)<clip.w*.88f;
            if(onScreen)p={cx+clip.x/clip.w*cx,cy+clip.y/clip.w*cy};
            else{if(glm::length(p)<.0001f)p={0,-1};p=glm::normalize(p);float r=std::min(cx*.90f/std::max(.001f,std::abs(p.x)),cy*.86f/std::max(.001f,std::abs(p.y)));p=glm::vec2(cx,cy)+p*r;}
            return std::pair{p,onScreen};
        };
        // A screen-space detail policy, independent of fleet size. Important
        // contacts always survive; duplicate ordinary markers share one place.
        std::array<std::array<uint64_t,2>,128> markers{};
        auto marker=[&](glm::vec2 p,bool test){
            int x0=std::clamp(int((p.x-20)*128/extent.x),0,127),x1=std::clamp(int((p.x+20)*128/extent.x),0,127);
            int y0=std::clamp(int((p.y-20)*128/extent.y),0,127),y1=std::clamp(int((p.y+30)*128/extent.y),0,127);
            for(int word=x0/64;word<=x1/64;++word){int lo=std::max(x0-word*64,0),hi=std::min(x1-word*64,63);uint64_t mask=(~uint64_t(0)<<lo)&(~uint64_t(0)>>(63-hi));
                if(test)for(int y=y0;y<=y1;++y)if(markers[y][word]&mask)return false;}
            for(int word=x0/64;word<=x1/64;++word){int lo=std::max(x0-word*64,0),hi=std::min(x1-word*64,63);uint64_t mask=(~uint64_t(0)<<lo)&(~uint64_t(0)>>(63-hi));
                for(int y=y0;y<=y1;++y)markers[y][word]|=mask;}
            return true;
        };
        radarVisible.assign(contacts.size(),0);int rosterRows=0;
        if(roster)for(auto& c:contacts)if(c.type==ContactShip || c.type==ContactStation)++rosterRows;
        if(radarMode!=Allegiance::c_rlNone)for(int priority=0;priority<2;++priority)for(size_t i=0;i<contacts.size();++i){
            const auto& c=contacts[i];bool key=important(c);if((key?0:1)!=priority)continue;
            auto [p,onScreen]=project(c);
            auto a=Allegiance::RadarPresentation(radarMode,c.type,key,onScreen,glm::distance(g.position,c.position)-g.radius-c.radius,1,c);
            if(!a.icon && !a.label && !a.stats && !a.range && !(g.commandView && c.commandSelected))continue;
            if(!marker(p,!key))continue;radarVisible[i]=1;
            if(onScreen){if(a.icon || (g.commandView && c.commandSelected))occupy({p.x-20,p.y-20,40,40},false);
                if(a.stats && (c.type==ContactShip || c.type==ContactStation))occupy({p.x-16,p.y+19,32,10},false);}
        }
        if(roster)occupy({extent.x-330,106,298,34+rosterRows*25.f},false);
        if(radarMode!=Allegiance::c_rlNone)for(int priority=0;priority<2;++priority)for(size_t i=0;i<contacts.size();++i){
            const auto& c=contacts[i];if(!radarVisible[i] || (important(c)?0:1)!=priority)continue;
            auto [p,onScreen]=project(c);
            float distance=glm::distance(g.position,c.position);
            auto appearance=Allegiance::RadarPresentation(radarMode,c.type,important(c),onScreen,distance-g.radius-c.radius,int(distance+.5f),c);
            if(!appearance.icon && !appearance.label && !appearance.stats && appearance.range==0)continue;
            auto color=c.threatFlash?red:c.neutral?muted:c.enemy?red:green;
            float bracket=0;
            if(c.selected || c.threat || (c.objective && c.type==ContactShip)) {
            float screenRadius=onScreen?std::max(7.f,Allegiance::IndicatorAngularRadius(c.radius,glm::distance(c.position,g.eyePosition),extent.x/horizontalFov)):7.f;
            float edge=std::min(std::min(p.x,extent.x-p.x),std::min(p.y,extent.y-p.y));
            bracket=std::min(1.05f*std::min(screenRadius,std::sqrt(2.f)*80.f),edge+10.f);
            }
            if(c.selected)expanded("targetedbmp",p,bracket,color);
            if(c.ordered)image("orderedbmp",p.x-16,p.y-16,color);
            if(g.commandView && c.commandSelected && onScreen)expanded("targetedbmp",p,18,gold);
            if(c.objective && c.type==ContactShip)expanded(c.artifact?"hasartifactbmp":"hasflagbmp",p,bracket,color);
            if(c.threat)expanded("lastfiredbmp",p,bracket,color);
            std::string_view icon=appearance.tech?"icontechbmp":c.icon;
            if(appearance.icon && !icon.empty()) {
                auto it=sprites.find(icon);
                if(it==sprites.end()){auto im=assets.image(std::string(icon));it=sprites.emplace(icon,Sprite{renderer.addTexture(im,!modern),float(im->width),float(im->height)}).first;}
                auto& s=it->second;rect(p.x-s.w/2,p.y-s.h/2,s.w,s.h,color,s.texture|hudAdditive);
            } else if(appearance.icon) {
            // Station/ship/rock symbols remain legible without stretching a
            // screen-sized sprite. Selected contacts carry hull/shield feedback.
            float r=c.type==ContactStation?6:4;
            rect(p.x-r,p.y-r,2*r,1.5f,color);rect(p.x-r,p.y+r,2*r,1.5f,color);
            rect(p.x-r,p.y-r,1.5f,2*r,color);rect(p.x+r,p.y-r,1.5f,2*r,color);
            }
            if(c.threat)text(p.x,p.y-30,"!",gold,Bold,0.5f);
            if(appearance.label || appearance.range) {
                snprintf(label,sizeof(label),"%d m",appearance.range);
                float w=std::max(appearance.label?std::min(width(c.name,Small),190.f):0.f,appearance.range?width(label,Small):0.f);
                float height=appearance.label && appearance.range?32.f:16.f;
                const glm::vec2 choices[]={{20,-13},{-20-w,-13},{-w*.5f,-34-height},{-w*.5f,32},{20,-50-height},{-20-w,44}};
                for(auto offset:choices)if(occupy({p.x+offset.x-2,p.y+offset.y-2,w+4,height+4},true)) {
                    if(appearance.label)text(p.x+offset.x,p.y+offset.y,c.name,color,Small,0,190);
                    if(appearance.range)text(p.x+offset.x,p.y+offset.y+(appearance.label?16:0),label,color,Small);break;
                }
            }
            if(appearance.stats && (c.type==ContactShip || c.type==ContactStation)){rect(p.x-14,p.y+21,28,2,muted);rect(p.x-14,p.y+21,28*c.hull,2,color);rect(p.x-14,p.y+25,28*c.shield,2,white);}
            if(c.selected && g.missiles && g.missileLock>0)expanded(g.missileLock==1.f?"lockedbmp":"plockbmp",p,bracket+40*(1-g.missileLock),glm::vec4(1));
            hits.push_back({{p.x-18,p.y-18,36,36},{HudCommand::Target,c.id,c.type}});
        }
        // Project a direction for the cockpit lead: translating it from the
        // player's position introduces parallax while looking from a turret.
        if(g.hasExternalAim) {
            auto p=vp*glm::vec4(g.aimPosition,1);
            if(p.w>0)image("centeroutbmp",cx+p.x/p.w*cx-32,cy+p.y/p.w*cy-32,
                g.inRange?glm::vec4(1-g.aimGreen,g.aimGreen,0,1):glm::vec4(.33f,.33f,.33f,.5f));
        } else {
            glm::vec4 color(.33f,.33f,.33f,.5f);
            if(g.hasWeaponSolution) {
                auto p=vp*glm::vec4(g.leadDirection,0);
                if(p.w>0) {
                    glm::vec2 lead{p.x/p.w*cx,p.y/p.w*cy};
                    if(g.hasLead)image(g.inRange?"leadinbmp":"leadoutbmp",cx+lead.x-6,cy+lead.y-6);
                    if(g.inRange) {
                        float radius=std::max(7.f,Allegiance::IndicatorAngularRadius(g.targetRadius,glm::distance(g.targetPosition,g.eyePosition),extent.x/horizontalFov));
                        float green=Allegiance::CockpitAimGreen(glm::dot(lead,lead),radius*radius);
                        color={1-green,green,0,1};
                    }
                }
            }
            image("centeroutbmp",cx-32,cy-32,color);
        }
        if(g.speed>1) {
            auto p=vp*glm::vec4(g.position+glm::normalize(g.velocity)*1000.0f,1);
            if(p.w>0 && std::abs(p.x)<p.w && std::abs(p.y)<p.w)image("directionbmp",cx+p.x/p.w*cx-16,cy+p.y/p.w*cy-16,muted);
        }
    }
    void inventoryPanel(const GameState& g) {
        if(!inventory)return;
        if(!modern)return;
        bool hasCargo=!g.equipment.empty() && g.equipment.back().mount<0,heading=false;
        float x=extent.x-36-214,y=extent.y-40-28-(g.equipment.size()+size_t(hasCargo))*18;
        inventoryBounds={x-11,y-24,225,24+28+(g.equipment.size()+size_t(hasCargo))*18};
        image("CortUI/classic/inventorytop.png",x,y,glm::vec4(1),false);
        text(x+107,y-24,g.ship,white,Bold,.5f);y+=14;
        image("CortUI/classic/inventorytab.png",x-11,y,glm::vec4(1),false);
        for(auto& e:g.equipment) {
            bool cargo=e.mount<0;
            if(cargo && !heading) {
                image("CortUI/classic/inventorylinebkgnd.png",x,y,glm::vec4(1),false);
                label({x+27,y,167,18},cargoKeys,muted);y+=18;heading=true;
            }
            image("CortUI/classic/inventorylinebkgnd.png",x,y,glm::vec4(1),false);
            if(e.selected)image("CortUI/classic/inventoryselected.png",x+7,y+1,glm::vec4(1),false);
            if(cursor && inside({x,y,214,18},pointer))rect(x+5,y,204,18,{0.2f,0.2f,0.3f,0.5f});
            char key[8],status[32];snprintf(key,sizeof(key),"%d",e.mount+1);status[0]=0;
            std::string_view shortcut;
            if(e.fixedWeapon && !g.passenger)shortcut=key;
            else if(!cargo && !g.passenger)switch(e.kind) {
                case PartKind::Missile:shortcut=keyNames[size_t(Binding::Missile)];break;
                case PartKind::Cloak:shortcut=keyNames[size_t(Binding::Cloak)];break;
                case PartKind::Chaff:shortcut=keyNames[size_t(Binding::Chaff)];break;
                case PartKind::Dispenser:shortcut=keyNames[size_t(Binding::Deploy)];break;
                default:break;
            }
            auto color=cargo?muted:e.empty?red:e.mounted<1?gold:white;
            if(e.kind==PartKind::Pack || e.kind==PartKind::Missile || e.kind==PartKind::Chaff || e.kind==PartKind::Dispenser)snprintf(status,sizeof(status),"%d",e.amount);
            else if(cargo)snprintf(status,sizeof(status),"Spare");
            else if(e.kind==PartKind::Shield || e.kind==PartKind::Booster)snprintf(status,sizeof(status),"%.0f%%",e.value*100);
            label({x+9,y,14,18},shortcut,gold);label({x+27,y,97,18},e.name,color);
            bool mounting=e.readiness==1,arming=e.readiness==2;
            if(mounting || arming) {
                float fraction=std::clamp(mounting?e.mounted:e.ready,0.0f,1.0f);
                rect(x+130,y+2,64,14,{.015f,.015f,.015f,1});rect(x+130,y+2,64*fraction,14,mounting?glm::vec4(.25f,.01f,.01f,1):glm::vec4(.22f,.22f,0,1));label({x+130,y+2,64,14},mounting?"Mounting":"Arming",white,Compact,.5f);
            }else label({x+130,y,74,18},status,color,Compact,1);
            if(cargo && !g.passenger)hits.push_back({{x,y,214,18},{HudCommand::Cargo,e.mount}});
            if(e.fixedWeapon && !g.passenger)hits.push_back({{x,y,214,18},{HudCommand::Weapon,e.mount}});
            y+=18;
        }
        image("CortUI/classic/inventorybottom.png",x,y,glm::vec4(1),false);
        if(g.passenger){char label[48];snprintf(label,sizeof(label),"Gunner / turret %d",g.turret+1);this->label({x,y+18,214,25},g.turret>=0?label:"Observer",gold);return;}
        button({x,y+18,100,25},groupKeys,{HudCommand::Weapon,-1},g.selectedWeapon<0,Compact);
        button({x+108,y+18,106,25},autopilotKeys,{HudCommand::Autopilot},g.autopilot,Compact);
    }
#include "hud_sectormap.inl"
    void contactPanel(std::span<const Contact> contacts) {
        if(!roster)return;float x=extent.x-326,y=110;
        rect(x,y,290,28,panel);text(x+10,y+5,contactsKeys,gold,Bold);y+=30;
        for(auto& c:contacts)if(c.type==ContactShip || c.type==ContactStation) {
            auto color=c.enemy?red:green;rect(x,y,290,24,panel);if(c.selected)rect(x,y,3,24,color);
            text(x+10,y+3,c.name,color,Normal,0,230);hits.push_back({{x,y,290,24},{HudCommand::Target,c.id,c.type}});y+=25;
        }
    }
    void steering(const GameState& g){
        float amount=glm::length(g.steering);if(!joystickIndicator || amount<.001f || (g.camera!=CameraMode::Cockpit && g.camera!=CameraMode::Chase && g.camera!=CameraMode::Missile))return;
        auto direction=glm::vec2(-g.steering.x,g.steering.y)/amount,p=extent*.5f+glm::vec2(-g.steering.x,g.steering.y)*100.f;
        auto normal=glm::vec2(-direction.y,direction.x);auto color=gold;color.a=std::min(amount,1.f);
        float size=8.f+std::min(amount,1.f)*8;line(p,p-direction*size+normal*size*.5f,2,color);line(p,p-direction*size-normal*size*.5f,2,color);
        line(p-direction*size+normal*size*.5f,p-direction*size-normal*size*.5f,2,color);
    }
    float chatHeight()const{return modern?23.f:float(chatFonts[size_t(chatSize-1)*2+1]->height);}
    void chatGeometry(uint32_t overlays) {
        int lines=std::max(1,(overlays&ofInLoadout)?std::min(chatLines,6):chatLines),height=int(chatHeight());
        chatPane=Allegiance::CommPanePresentation(chatLines,height,overlays&ofInLoadout);
        if(!modern){
            int key=lines*4+chatSize;
            if(key!=chatGeometryKey){
                chatControls=chatUi->imageLayout("dialog","cortCommPaneImage",{{"ChatFontSize",chatSize},{"NumChatLines",lines}}, {},{{"chatListPane",{chatContentWidth,chatPane.listHeight}}});
                auto host=std::find_if(chatControls.begin(),chatControls.end(),[](auto& c){return c.name=="chatListPane";});
                if(host==chatControls.end())throw std::runtime_error("Authored CommPane contains no chat list");
                chatHostBounds=host->bounds;chatHostClip=host->clip.value_or(host->bounds);
                for(auto& control:chatControls)if(control.image)authoredTextures.emplace(control.image.get(),renderer.addTexture(control.image,true));
                chatGeometryKey=key;
            }
        }
        messagePager.setRows(modern?MessagePager::rows:chatPane.slots);
    }
    glm::vec2 chatAnchor() {
        HudValues values;values[HudNumber::ChatLines]=chatLines;auto entries=chatLayouts[hudStyle]->evaluate(values,extent);
        return entries.empty()?glm::vec2(0):glm::floor(entries.front().on);
    }
    float chatWidth(std::string_view value,bool leader=false)const {
        if(modern)return width(value,Small);
        const auto& font=*chatFonts[size_t(chatSize-1)*2+size_t(leader)];float result=0;
        for(size_t i=0;i<value.size();){if(auto bytes=Allegiance::TextColorBytes(value.substr(i))){i+=bytes;continue;}result+=font.glyphs[next(value,i)].z;}
        return result;
    }
    void chatText(float x,float y,std::string_view value,glm::vec4 color,bool leader,const glm::vec4* clip=nullptr) {
        if(modern){text(x,y,value,color,Small);return;}
        size_t index=size_t(chatSize-1)*2+size_t(leader);const auto& font=*chatFonts[index];
        auto baseColor=color;
        for(size_t i=0;i<value.size();){auto remaining=value.substr(i);Allegiance::InlineTextColor code;if(Allegiance::ConsumeTextColor(remaining,code)){i=value.size()-remaining.size();color=inlineTextColor(code,color,baseColor);continue;}unsigned ch=next(value,i);auto g=font.glyphs[ch];
            glm::vec2 lo{x,y},hi{x+g.z,y+g.w};
            if(clip){lo=glm::max(lo,glm::vec2(*clip));hi=glm::min(hi,glm::vec2(*clip)+glm::vec2(clip->z,clip->w));}
            if(hi.x>lo.x && hi.y>lo.y){glm::vec2 atlas{font.coverage->width,font.coverage->height};
                rect(lo.x,lo.y,hi.x-lo.x,hi.y-lo.y,color,chatTextures[index],{(glm::vec2(g)+lo-glm::vec2(x,y))/atlas,(glm::vec2(g)+hi-glm::vec2(x,y))/atlas});}
            x+=g.z;}
    }
    void layoutMessages(const GameState& g) {
        uint64_t first=g.messages.empty()?0:g.messages.front().serial,last=g.messages.empty()?0:g.messages.back().serial;
        uint64_t revision=g.messages.empty()?0:g.messages.back().displayRevision;
        if(messageScale==scale && messageFont==fontGeneration && messageFirst==first && messageLast==last && messageRevision==revision && messageCount==g.messages.size())return;
        ++messageUpdates;
        bool append=messageScale==scale && messageFont==fontGeneration && messageRevision==revision && last>=messageLast;
        auto begin=g.messages.begin();
        if(append) {
            auto retained=std::lower_bound(messageLines.begin(),messageLines.end(),first,[](const auto& line,uint64_t id){return (line.serial>>16)<id;});
            messageLines.erase(messageLines.begin(),retained);
            begin=std::upper_bound(g.messages.begin(),g.messages.end(),messageLast,[](uint64_t id,const auto& message){return id<message.serial;});
        } else messageLines.clear();
        for(auto it=begin;it!=g.messages.end();++it) {
            const auto& message=*it;
            if(!modern && message.tip)continue;
            std::string_view display=message.displayRevision?message.displayText:message.text;
            if(modern)wrapText(display,400.f,chatWidth("  ",message.leader),[&](std::string_view glyph){return chatWidth(glyph,message.leader);},wrappedLines);
            else {const auto& font=*chatFonts[size_t(chatSize-1)*2+size_t(message.leader)];wrapOriginalChat(display,int(chatContentWidth)-6,[&](unsigned char ch){return font.glyphs[ch].z;},wrappedLines);}
            for(size_t i=0;i<wrappedLines.size();++i) {
                auto& line=wrappedLines[i];auto color=modern?message.color:authoredColor(message.warning?red:message.color);
                messageLines.push_back({(message.serial<<16)|i,message.displayTime?message.displayTime:message.time,display.substr(line.offset,line.length),message.warning,line.continued,message.displayTime!=0,color,message.leader});
            }
        }
        messageFont=fontGeneration;messageFirst=first;messageLast=last;messageRevision=revision;messageCount=g.messages.size();messageScale=scale;
    }
    void messages(const GameState& g,float top) {
        if(modern && g.camera!=CameraMode::Cockpit)text(extent.x/2,14,cameraLabels[int(g.camera)],muted,Small,.5f);
        double now=uiTime();auto offset=messagePager.offset();
        float lineHeight=chatHeight();
        auto anchor=modern?glm::vec2(0):chatAnchor();
        float x=modern?extent.x/2-200:anchor.x+chatHostBounds.x+3,y=modern?top:anchor.y+chatHostBounds.y+3;
        size_t end=messageLines.size()-(log?offset:0),rows=modern?(log?MessagePager::rows:4):messagePager.pageRows();
        size_t start=end>rows?end-rows:0;
        auto shown=[&](const auto& line){return !modern || log || Allegiance::MessageVisible(line.warning,g.trainingMission!=0,(line.wallClock?now:g.time)-line.time);};
        size_t count=0;for(size_t i=start;i<end;++i)if(shown(messageLines[i]))++count;
        glm::vec4 clip=chatHostClip;clip.x+=anchor.x;clip.y+=anchor.y;
        if(modern){if(count || log)rect(x-8,y-2,416,float(count)*23+(log?30:2),{.005f,.009f,.018f,log?.94f:.68f});}
        else {
            auto host=chatHostBounds;host.x+=anchor.x;host.y+=anchor.y;
            auto lo=glm::max(glm::vec2(clip),glm::vec2(host)),hi=glm::min(glm::vec2(clip)+glm::vec2(clip.z,clip.w),glm::vec2(host)+glm::vec2(host.z,host.w));
            clip={lo,glm::max(hi-lo,glm::vec2(0))};chatBounds=clip;chatScale=scale;
            for(auto& control:chatControls)if(control.image && control.bounds.z>0 && control.bounds.w>0){
                auto lo=glm::vec2(control.bounds),hi=lo+glm::vec2(control.bounds.z,control.bounds.w),old=lo;
                if(control.clip){lo=glm::max(lo,glm::vec2(*control.clip));hi=glm::min(hi,glm::vec2(*control.clip)+glm::vec2(control.clip->z,control.clip->w));}
                if(hi.x>lo.x && hi.y>lo.y){auto uv=glm::vec2(control.uv),span=glm::vec2(control.uv.z,control.uv.w)-uv,size=glm::vec2(control.bounds.z,control.bounds.w);
                    rect(anchor.x+lo.x,anchor.y+lo.y,hi.x-lo.x,hi.y-lo.y,control.color,authoredTextures.at(control.image.get()),{uv+(lo-old)/size*span,uv+(hi-old)/size*span});}
            }
        }
        for(size_t i=start;i<end;++i) {
            const auto& line=messageLines[i];if(!shown(line))continue;chatText(x+(line.continued?chatWidth("  ",line.leader):0),y,line.text,modern && line.warning?red:line.color,line.leader,modern?nullptr:&clip);y+=lineHeight;
        }
        if(log && modern)text(x,y+4,historyKeys,muted,Small);
    }
    void cacheCaptions() {
        auto key=[&](Binding b)->const std::string&{return keyNames[size_t(b)];};
        flightHelp={{{"Mouse / arrows","Turn"},{key(Binding::ThrottleUp)+" / "+key(Binding::ThrottleDown)+" / wheel","Throttle"},
            {key(Binding::Left)+" / "+key(Binding::Right),"Strafe"},{key(Binding::RollLeft)+" / "+key(Binding::RollRight),"Roll"},
            {key(Binding::Up)+" / "+key(Binding::Down),"Up / down"},{key(Binding::Boost),"Afterburner"},
            {key(Binding::Coast),"Vector lock"},{"Left mouse","Selected guns"},{"R mouse / "+key(Binding::Missile),"Missile"},
            {key(Binding::Chaff)+" / "+key(Binding::Deploy),"Chaff / deploy"},{key(Binding::Cloak),"Cloak"},{"+ / -","HUD size"},
            {key(Binding::Forward)+" / "+key(Binding::Backward),"Forward / reverse"},{key(Binding::Ripcord),"Ripcord / cancel"},
            {key(Binding::Eject),"Eject / abandon"},{key(Binding::Reload),"Reload reserves"}}};
        interfaceHelp={{{key(Binding::Target),"Next target"},{"1 - 4 / 0","Gun / group fire"},
            {key(Binding::Autopilot),"Autopilot to target"},{key(Binding::ToggleView),"Cockpit / chase"},{key(Binding::PaneInventory),"Inventory"},{key(Binding::PaneContacts),"Contacts"},
            {key(Binding::SectorMap),"Sector map"},{key(Binding::EventLog),"Event log"},{key(Binding::RadarDetail),"Radar detail"},{key(Binding::HudVisible)+" / "+key(Binding::Diagnostics),"HUD / timing"},
            {key(Binding::Cursor),"Cursor / flight"},{key(Binding::Intro),"Intro movie"},{key(Binding::Inventory),"Cargo / equipment"},
            {key(Binding::NextCargo),"Next cargo stack"},{key(Binding::DropCargo),"Drop cargo"},{key(Binding::Pause),"Pause / menu"}}};
        queuedKeys=key(Binding::AcceptOrder)+" accept / "+key(Binding::ExecuteOrder)+" execute / "+key(Binding::RejectOrder)+" reject";
        moneyKeys=key(Binding::AcceptOrder)+" approve / "+key(Binding::RejectOrder)+" decline";
        cargoKeys=key(Binding::Inventory)+"  Cargo reserves";groupKeys=key(Binding::GroupFire)+"  Group fire";
        autopilotKeys=key(Binding::Autopilot)+"  Autopilot";contactsKeys="CONTACTS  /  "+key(Binding::PaneContacts);
        teamKeys=key(Binding::PaneContacts)+"  Team / Take me to";
        cursorKeys="CURSOR  /  click a contact or weapon  /  "+key(Binding::Cursor)+" to fly";
        flightKeys=key(Binding::Help)+" help   "+key(Binding::PaneInventory)+" inventory   "+key(Binding::PaneContacts)+" contacts   "+key(Binding::SectorMap)+" map   "+key(Binding::QuickChat)+" quick chat";
        historyKeys="EVENT LOG  /  "+key(Binding::ChatPageUp)+" / "+key(Binding::ChatPageDown);
        constexpr const char* modes[]={"None","All","Default","Target","Minimal"};
        for(size_t i=0;i<radarLabels.size();++i)radarLabels[i]=std::string("Radar: ")+modes[i]+" ["+key(Binding::RadarDetail)+"]";
        for(size_t i=0;i<cameraLabels.size();++i)cameraLabels[i]=std::string(cameraModeNames[i])+" view";
        helpFooters[0]=key(Binding::CameraZoomIn)+" / "+key(Binding::CameraZoomOut)+": distance   /   Alt+"+key(Binding::ToggleView)+": cycle views";
        helpFooters[1]="Alt + turn / roll controls: rotate external view   /   "+key(Binding::Help)+": close";
        helpFooters[2]=key(Binding::PilotOrders)+": pilot commands   /   "+key(Binding::AcceptOrder)+": accept   /   "+key(Binding::ExecuteOrder)+": execute   /   "+key(Binding::RejectOrder)+": reject";
        helpFooters[3]=key(Binding::TargetOrder)+": target order   /   "+key(Binding::ClearOrder)+": clear order   /   "+key(Binding::MatchSpeed)+": match speed";
    }
    void updateCounters(const GameState& g) {
        std::array<int,3> counters{g.kills,g.deaths,g.lives};if(counters==lastCounters)return;lastCounters=counters;
        char label[128];if(g.lives<10)snprintf(label,sizeof(label),"KILLS %d   DEATHS %d   LIVES %d",g.kills,g.deaths,std::max(0,g.lives));
        else snprintf(label,sizeof(label),"KILLS %d   DEATHS %d",g.kills,g.deaths);counterText=label;
    }
    void modal(const GameState& g,bool paused,bool help) {
        float cx=extent.x/2,cy=extent.y/2;
        if(!g.sideActive) {
            rect(cx-355,cy-125,710,238,{panel.r,panel.g,panel.b,1});
            text(cx,cy-108,"TEAM ELIMINATED",gold,Title,.5f);
            text(cx,cy-72,"No restart station remains for your team.",white,Normal,.5f);
            text(cx,cy-44,g.complete?"The match has ended.":"The remaining teams are still playing.",muted,Normal,.5f);
            text(cx,cy-14,"Chat, team scores and configuration remain available.",muted,Small,.5f);
            button({cx-275,cy+40,260,38},"Team scores",{HudCommand::Scores});
            button({cx+15,cy+40,260,38},"Session menu",{HudCommand::SessionMenu});
            return;
        }
        rect(cx-355,cy-305,710,610,{panel.r,panel.g,panel.b,1});
        std::string_view title=g.dead?"SHIP DESTROYED":g.docked?"DOCKED":paused?"PAUSED":"FLIGHT CONTROLS";
        text(cx,cy-287,title,gold,Title,0.5f);
        for(size_t i=0;i<flightHelp.size();++i) {
            float y=cy-242+i*23;
            text(cx-325,y,flightHelp[i][0],muted,Normal,0,139);text(cx-178,y,flightHelp[i][1],white,Normal);
            text(cx+25,y,interfaceHelp[i][0],muted,Normal,0,117);text(cx+150,y,interfaceHelp[i][1],white,Normal);
        }
        text(cx,cy+133,helpFooters[0],muted,Small,0.5f);
        text(cx,cy+156,helpFooters[1],muted,Small,0.5f);
        text(cx,cy+182,helpFooters[2],white,Small,.5f);
        text(cx,cy+202,helpFooters[3],muted,Small,.5f);
        if(g.dead)button({cx-300,cy+239,190,32},"Return to hangar",{HudCommand::Respawn});
        button({cx-85,cy+239,170,32},"Return to base",{HudCommand::NextHull});
        if(g.docked)button({cx+100,cy+239,170,32},"Enter  Launch",{HudCommand::Launch});
        else text(cx+185,cy+248,"Local session",muted,Small,0.5f);
    }
};
Hud::Hud(Renderer& r,Assets& a,float scale):impl_(std::make_unique<Impl>(r,a,scale)){}
Hud::~Hud()=default;
void Hud::preloadIcons(std::span<const std::string_view> icons) {
    auto& h=*impl_;for(auto name:icons)if(h.sprites.find(name)==h.sprites.end()) {
        auto im=h.assets.image(std::string(name));h.sprites.emplace(name,Sprite{h.renderer.addTexture(im,!h.modern),float(im->width),float(im->height)});
    }
}
void Hud::preloadInventory(std::span<const std::string_view> scripts) {
    OriginalUi ui(impl_->assets);for(auto name:scripts)impl_->preloadInventory(ui,name);
}
void Hud::preload(std::span<const Contact> contacts) {
    auto& h=*impl_;for(auto& c:contacts)if(!c.icon.empty() && h.sprites.find(c.icon)==h.sprites.end()) {
        auto im=h.assets.image(std::string(c.icon));h.sprites.emplace(c.icon,Sprite{h.renderer.addTexture(im,!h.modern),float(im->width),float(im->height)});
    }
}
void Hud::preloadSlides(std::span<const TrainingSlide> slides) {
    auto& h=*impl_;
    for(auto& slide:slides)for(auto& c:slide.view.artwork){
        if(c.image)h.renderer.addTexture(c.image,true);
        for(auto& image:c.animation)h.renderer.addTexture(image,true);
        if(c.kind==OriginalControl::Kind::Text){auto font=h.assets.font(c.font.empty()?"smallFont":c.font,c.fontHeight,c.fontBold);h.renderer.addTexture(font->coverage,true);}
    }
}
std::span<const HudQuad> Hud::buildSlide(glm::ivec2 size,TrainingSlide& slide,size_t index,size_t count) {
    auto& h=*impl_;slide.view.classicScale=classicInterfaceScale(h.classicScale,h.displayScale);
    buildMenu(size,slide.view,{-1,-1});
    if(h.modern){auto origin=originalMenuLayout(size,slide.view.classicScale).origin;
        h.rect(origin.x,origin.y+576,800,24,{0,0,0,.8f});
        h.text(origin.x+14,origin.y+580,"Any key or click: next slide",white,Compact);
        h.text(origin.x+786,origin.y+580,std::to_string(std::min(index+1,count))+" / "+std::to_string(count),white,Compact,1);}
    return h.quads;
}
void Hud::setScale(float scale){impl_->userScale=std::clamp(scale,0.5f,2.5f);}
void Hud::displayScale(float scale){impl_->displayScale=scale;}
void Hud::setDiagnostics(bool enabled){impl_->diagnostics=enabled;}
void Hud::modernInterface(bool enabled){if(impl_->modern!=enabled)impl_->messageFont=0;impl_->modern=enabled;}
void Hud::configure(const Settings& s){setScale(s.uiScale);setDiagnostics(s.showTiming);auto& h=*impl_;h.classicScale=s.classicUiScale;h.joystickIndicator=s.joystickIndicator;h.visual=s.visual;h.cockpitRadar=s.cockpitRadar;h.commandRadar=s.commandRadar;
    h.hudStyle=s.hudStyle;
    if(h.chatSize!=s.chatSize){h.messageFont=0;h.messagePager.reset();}if(h.chatLines!=s.chatLines)h.messagePager.reset();h.chatSize=s.chatSize;h.chatLines=s.chatLines;
    for(size_t i=0;i<s.keys.size();++i)h.keyNames[i]=keyName(s,Binding(i));
    h.cacheCaptions();
}
MessageLayoutStats Hud::messageLayoutStats()const{return {impl_->messageLines.size(),impl_->messagePager.offset(),impl_->messageUpdates,impl_->messagePager.pageRows(),impl_->chatHeight(),impl_->chatBounds*impl_->chatScale,impl_->messagePager.following()};}
void Hud::resetMessages(){impl_->messagePager.reset();impl_->messageLines.clear();impl_->messageFont=0;impl_->chatBounds={};}
void Hud::pageMessages(const GameState& g,bool older){impl_->log=true;impl_->layoutMessages(g);impl_->messagePager.page(impl_->messageLines,uiTime(),older);}
int Hud::cycleRadar(bool commandView){auto& h=*impl_;auto& mode=commandView?h.commandRadar:h.cockpitRadar;h.radarMode=mode=Allegiance::CycleRadarDetail(mode);return mode;}
bool Hud::binding(Binding binding) {
    auto& h=*impl_;switch(binding) {
    case Binding::HudLarger:setScale(h.userScale+0.125f);break;
    case Binding::HudSmaller:setScale(h.userScale-0.125f);break;
    case Binding::PaneInventory:h.inventory=!h.inventory;break;
    case Binding::PaneContacts:h.roster=!h.roster;break;
    case Binding::SectorMap:h.map=!h.map;break;
    case Binding::EventLog:h.log=!h.log;h.messagePager.reset();break;
    case Binding::Diagnostics:h.diagnostics=!h.diagnostics;break;
    case Binding::HudVisible:h.visible=!h.visible;break;
    case Binding::RadarDetail:cycleRadar(h.commandRadarView);break;
    default:return false;
    }return true;
}
bool Hud::key(SDL_Keycode key) {
    switch(key) {
    case SDLK_EQUALS:case SDLK_PLUS:return binding(Binding::HudLarger);
    case SDLK_MINUS:return binding(Binding::HudSmaller);
    case SDLK_F4:return binding(Binding::PaneInventory);case SDLK_F6:return binding(Binding::PaneContacts);
    case SDLK_F7:return binding(Binding::SectorMap);case SDLK_F8:return binding(Binding::EventLog);
    case SDLK_F9:return binding(Binding::Diagnostics);case SDLK_H:return binding(Binding::HudVisible);
    case SDLK_Z:return binding(Binding::RadarDetail);default:return false;
    }
}
void Hud::sectorInfoService(std::function<const SectorInfo&(int)> service){impl_->sectorInfoService=std::move(service);}
void Hud::cancelPointer(){impl_->mapDragging=false;}
bool Hud::pointerCaptured()const{return impl_->mapDragging;}
bool Hud::mapPointerMove(glm::vec2 p){auto& h=*impl_;if(!h.mapDragging)return false;
    p/=h.mapHitScale;float span=h.mapBounds.maximum.x-h.mapBounds.minimum.x;
    h.mapDrag.x+=span*(h.mapLastPointer.x-p.x)/h.mapViewport.z;
    // The original drag uses the X extent/width for both axes.
    h.mapDrag.y+=span*(p.y-h.mapLastPointer.y)/h.mapViewport.z;h.mapLastPointer=p;return true;
}
HudPointerResult Hud::mapPointerButton(glm::vec2 p,int button,bool down,bool control){
    auto& h=*impl_;if(button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT)return {};
    p/=h.mapHitScale;
    if(!down && h.mapDragging && button==SDL_BUTTON_LEFT){h.mapDragging=false;return {true,false,{}};}
    if(!h.mapActive && !h.mapDragging)return {};
    if(!h.mapDragging && inside(h.mapExpandBounds,p)){
        // SectorInfoPane is release-triggered and does not capture a press.
        if(button==SDL_BUTTON_LEFT && !down)h.mapExpanded=!h.mapExpanded;
        return {true,false,{}};
    }
    if(!inside(h.mapViewport,p))return h.mapDragging?HudPointerResult{true,true,{}}:HudPointerResult{};
    int sector=h.mapSectorAt(p);
    if(down && sector>=0)return {true,h.mapDragging,HudAction{control && button==SDL_BUTTON_RIGHT?HudCommand::SectorHighlight:button==SDL_BUTTON_RIGHT?HudCommand::SectorChat:HudCommand::Sector,sector}};
    if(down && button==SDL_BUTTON_LEFT && h.mapBounds.canDrag){h.mapDragging=true;h.mapLastPointer=p;return {true,true,{}};}
    return {true,h.mapDragging,{}};
}
std::optional<std::string_view> Hud::mapCursor(glm::vec2 p,bool canOrder)const {
    const auto& h=*impl_;
    if(inside(h.inventoryBounds,p/h.hitScale))return "default";
    if(inside(h.commandBounds,p/h.hitScale))return "default";
    if(!h.mapActive)return {};p/=h.mapHitScale;
    if(h.mapDragging)return "drag";
    if(!inside(h.mapViewport,p))return inside(h.mapPanelBounds,p)?std::optional<std::string_view>("default"):std::nullopt;
    if(h.mapSectorAt(p)>=0)return canOrder?"goto":"default";
    return h.mapBounds.canDrag?"drag":"default";
}
std::span<const HudQuad> Hud::appendPointer(glm::ivec2 size,glm::vec2 pointer,const Contact* hovered,const OriginalControl* fallbackCursor,glm::vec4 color) {
    auto& h=*impl_;h.resize(size);pointer/=h.scale;
    if(hovered) {
        for(int axis=0;axis<3;++axis){float value=color[axis];color[axis]=value<=.04045f?value/12.92f:std::pow((value+.055f)/1.055f,2.4f);}
        auto font=h.assets.font("smallFont");auto width=Allegiance::TextExtent(hovered->name,[&](unsigned char ch){return font->glyphs[ch].z;});
        h.bitmapText("smallFont",std::floor(pointer.x-width*.5f),std::floor(pointer.y+8+font->height),hovered->name,color,h.extent.x);
    }
    if(fallbackCursor) {
        const auto& c=*fallbackCursor;auto texture=h.renderer.addTexture(c.image,true);
        h.rect(std::floor(pointer.x)+c.bounds.x,std::floor(pointer.y)+c.bounds.y,c.bounds.z,c.bounds.w,glm::vec4(1),texture);
    }
    return h.quads;
}
std::optional<HudAction> Hud::click(glm::vec2 pixelPosition) {
    auto& h=*impl_;auto p=pixelPosition/h.hitScale;
    for(auto it=h.hits.rbegin();it!=h.hits.rend();++it)if(inside(it->bounds,p))return it->action;return {};
}
void Hud::consoleControls(int recipient,std::array<bool,9> verbs){impl_->consoleRecipient=recipient;impl_->consoleVerbs=verbs;}
const ModelPreview* Hud::preview()const{return impl_->preview?&*impl_->preview:nullptr;}
std::span<const HudQuad> Hud::buildMenu(glm::ivec2 size,const MenuView& menu,glm::vec2 pointer,bool append) {
    auto& h=*impl_;h.resize(size);if(!append){h.mapActive=false;h.inventoryBounds={};h.commandBounds={};h.chatBounds={};h.quads.clear();h.preview.reset();h.hits.clear();h.hitScale=h.scale;}h.pointer=pointer/h.scale;h.cursor=true;
    if(menu.original) {
        auto layout=originalMenuLayout(size,menu.classicScale,menu.canvas);h.scale=layout.scale;h.extent=glm::vec2(size)/h.scale;h.pointer=layout.local(pointer);
        if(!menu.overlay)h.rect(0,0,h.extent.x,h.extent.y,{0,0,0,1});
        const double menuTime=uiTime();
        auto text=[&](const OriginalControl& c,std::string_view value,const glm::vec4* clip=nullptr,glm::vec2 translation=glm::vec2(0)) {
            auto bounds=c.bounds;bounds.x+=translation.x;bounds.y+=translation.y;
            if(clip && (bounds.y>=clip->y+clip->w || bounds.y+bounds.w<=clip->y))return;
            auto font=h.assets.font(c.font.empty()?"smallFont":c.font,c.fontHeight,c.fontBold);uint32_t texture=h.renderer.addTexture(font->coverage,true)|(c.additive?hudAdditive:0);
            bool edit=c.textMode==OriginalControl::TextMode::Edit,password=edit && c.editPassword;
            auto advance=[&](unsigned char ch){return font->glyphs[password?'*':ch].z;};
            auto measure=[&](std::string_view s){if(!password)return Allegiance::TextExtent(s,advance);float w=0;for(unsigned char ch:s)w+=advance(ch);return w;};
            float y=std::floor(bounds.y);std::string_view remaining=value;
            if(edit){remaining=Allegiance::ClipEditText(remaining,int(bounds.z),c.editFocused,c.editReadOnly,advance,int(font->glyphs['o'].z));float free=bounds.w-font->height;if(free>1)y+=std::floor(free/2);}
            auto editRow=remaining;
            while(!remaining.empty() && (c.textMode==OriginalControl::TextMode::Image || y<bounds.y+std::max(bounds.w,float(font->height)))) {
                std::string_view row;size_t consumed=remaining.size();
                if(edit)row=remaining;
                else if(c.textMode==OriginalControl::TextMode::Pane){
                    row=c.clipCharacters?Allegiance::ClipPaneText(remaining,int(bounds.z),c.justify,advance):remaining;
                }else if(c.textMode==OriginalControl::TextMode::Image){
                    auto line=Allegiance::ImageLine(remaining,int(bounds.z),advance);row=remaining.substr(0,line.length);consumed=line.consumed;
                }else {
                    size_t end=remaining.find('\n');if(end==remaining.npos)end=remaining.size();row=remaining.substr(0,end);consumed=end+(end<remaining.size());
                    if(c.clipCharacters && bounds.z>0 && measure(row)>bounds.z){
                        if(c.singleLine)row=Allegiance::ClipPaneText(row,int(bounds.z),c.justify,advance);
                        else {auto line=Allegiance::ImageLine(remaining,int(bounds.z),advance);row=remaining.substr(0,line.length);consumed=line.consumed;}
                    }
                }
                float x=std::floor(bounds.x+(c.justify==1?(bounds.z-measure(row))*.5f:c.justify==2?bounds.z-measure(row):0));
                auto color=authoredColor(c.color);
                auto baseColor=color;
                while(!row.empty()){Allegiance::InlineTextColor code;if(!password && Allegiance::ConsumeTextColor(row,code)){color=inlineTextColor(code,color,baseColor);continue;}auto ch=static_cast<unsigned char>(row.front());row.remove_prefix(1);auto g=font->glyphs[password?'*':ch];glm::vec4 uv{g.x/font->coverage->width,g.y/font->coverage->height,(g.x+g.z)/font->coverage->width,(g.y+g.w)/font->coverage->height};
                    glm::vec2 lo{x,y},hi{x+g.z,y+g.w};if(clip){lo=glm::max(lo,glm::vec2(*clip));hi=glm::min(hi,glm::vec2(*clip)+glm::vec2(clip->z,clip->w));}
                    if(hi.x>lo.x && hi.y>lo.y){auto span=glm::vec2(uv.z,uv.w)-glm::vec2(uv);auto lower=glm::vec2(uv)+(lo-glm::vec2(x,y))/glm::vec2(g.z,g.w)*span,upper=glm::vec2(uv)+(hi-glm::vec2(x,y))/glm::vec2(g.z,g.w)*span;h.rect(layout.origin.x+lo.x,layout.origin.y+lo.y,hi.x-lo.x,hi.y-lo.y,color,texture,{lower,upper});}x+=g.z;}
                remaining.remove_prefix(consumed);y+=font->height;
                if(c.singleLine || c.textMode==OriginalControl::TextMode::Pane)break;
            }
            if(edit && c.editFocused && Allegiance::EditCursorVisible(menuTime,c.editLastTyped)){
                float used=measure(editRow);glm::vec2 lo{bounds.x+used+1,bounds.y+1},hi{bounds.x+used+font->glyphs['o'].z,bounds.y+bounds.w-2};
                if(clip){lo=glm::max(lo,glm::vec2(*clip));hi=glm::min(hi,glm::vec2(*clip)+glm::vec2(clip->z,clip->w));}
                if(hi.x>lo.x && hi.y>lo.y)h.rect(layout.origin.x+lo.x,layout.origin.y+lo.y,hi.x-lo.x,hi.y-lo.y,authoredColor(c.color));
            }
        };
        // Update only animation state each frame; images and glyphs stay in the
        // renderer's shared bindless texture cache and one ordered HUD batch.
        static constexpr float secondsPerNs=1e-9f;
        auto now=SDL_GetTicksNS();
        auto slide=menuSlideOffset(menu,size,menuTime);
        auto captured=menu.capturedButton();
        h.menuHoverValues.clear();
        auto hoverValue=[&](std::string_view site,int initial){auto i=std::find_if(h.menuHoverValues.begin(),h.menuHoverValues.end(),[&](const auto& entry){return entry.first==site;});return i==h.menuHoverValues.end()?initial:i->second;};
        auto active=[&](const OriginalControl& c){for(const auto& condition:c.hoverConditions){int value=hoverValue(condition.site,condition.initialValue);
            if(condition.value?value!=*condition.value:std::find(condition.cases->begin(),condition.cases->end(),value)!=condition.cases->end())return false;}return true;};
        for(size_t index=0;index<menu.artwork.size();++index){const auto& c=menu.artwork[index];if(c.hoverBlocked || c.helpSite.empty() || !active(c))continue;
            if(menu.slide && menu.slide->undetectable && menu.slide->movesArtwork(index))continue;
            if(captured && c.name!=captured->control && c.hoverTarget!=captured->control)continue;
            if(c.kind==OriginalControl::Kind::Hover && !c.hoverTarget.empty() && std::none_of(menu.artwork.begin(),menu.artwork.end(),[&](const auto& target){return target.name==c.hoverTarget;}))continue;
            if(c.kind!=OriginalControl::Kind::Hover && std::none_of(menu.items.begin(),menu.items.end(),[&](const auto& item){return item.control==c.name && (item.enabled || item.detectWhenDisabled) && !item.blocked;}))continue;
            auto translation=menu.slide && menu.slide->movesArtwork(index)?slide:glm::vec2(0);auto bounds=c.bounds;bounds.x+=translation.x;bounds.y+=translation.y;auto clip=c.clip;if(clip){clip->x+=translation.x;clip->y+=translation.y;}
            // Enter/leave host values stay with the captured pane while its
            // face receives MouseMove/inside changes, as in Pane::HitTest.
            if(captured || (inside(bounds,h.pointer) && (!clip || inside(*clip,h.pointer)))){auto i=std::find_if(h.menuHoverValues.begin(),h.menuHoverValues.end(),[&](const auto& entry){return entry.first==c.helpSite;});if(i==h.menuHoverValues.end())h.menuHoverValues.emplace_back(c.helpSite,c.helpID);else i->second=c.helpID;}
        }
        for(size_t control=0;control<menu.artwork.size();++control) {
            const auto& c=menu.artwork[control];
            if(!active(c))continue;
            auto translation=menu.slide && menu.slide->movesArtwork(control)?slide:glm::vec2(0);
            auto bounds=c.bounds;bounds.x+=translation.x;bounds.y+=translation.y;
            auto clip=c.clip;if(clip){clip->x+=translation.x;clip->y+=translation.y;}
            if(c.name=="loadoutGeoPane" && menu.shipPreview && bounds.z>0 && bounds.w>0){
                ModelPreview p;p.item.model=menu.shipPreview->id;float t=float(SDL_GetTicksNS()*1e-9);
                auto rotation=glm::rotate(glm::mat4(1),glm::pi<float>()/8,glm::vec3(1,0,0))*glm::rotate(glm::mat4(1),t,glm::vec3(0,1,0));
                p.item.transform=glm::scale(rotation,glm::vec3(-1,1,-1)/std::max(.001f,menu.shipPreview->radius));
                auto projection=glm::perspectiveRH_ZO(glm::radians(50.f),bounds.z/bounds.w,10.f,.05f);projection[1][1]*=-1;
                p.viewProjection=projection*glm::translate(glm::mat4(1),glm::vec3(0,0,-2.4f));
                p.bounds={std::round((layout.origin.x+bounds.x)*h.scale),std::round((layout.origin.y+bounds.y)*h.scale),std::round(bounds.z*h.scale),std::round(bounds.w*h.scale)};
                if(clip)p.clip=glm::ivec4(glm::round(glm::vec4((layout.origin+glm::vec2(*clip))*h.scale,clip->z*h.scale,clip->w*h.scale)));
                p.overlayOffset=h.quads.size();h.preview=p;
            }
            if(c.kind==OriginalControl::Kind::Hover)continue;
            const MenuItem* item=nullptr;size_t index=0;
            for(size_t i=0;i<menu.items.size();++i)if(menu.items[i].control==c.name){item=&menu.items[i];index=i;break;}
            bool detectable=!(menu.slide && menu.slide->undetectable && menu.slide->movesArtwork(control));
            bool pointerInside=item && !item->blocked && detectable && (!captured || item==captured) && inside(bounds,h.pointer) && (!clip || inside(*clip,h.pointer));
            bool hover=pointerInside && item->enabled,down=pointerInside && int(index)==menu.pressed;
            if(c.kind==OriginalControl::Kind::Text) {text(c,c.text,clip?&*clip:nullptr,translation);continue;}
            if(c.kind==OriginalControl::Kind::Rectangle){auto color=c.color;if(c.name.starts_with("loadoutChoice") && (hover || (item && int(index)==menu.focus)))color={197.f/255,0,41.f/255,1};auto lo=glm::vec2(bounds),hi=lo+glm::vec2(bounds.z,bounds.w);if(clip){lo=glm::max(lo,glm::vec2(*clip));hi=glm::min(hi,glm::vec2(*clip)+glm::vec2(clip->z,clip->w));}if(hi.x>lo.x && hi.y>lo.y)h.rect(layout.origin.x+lo.x,layout.origin.y+lo.y,hi.x-lo.x,hi.y-lo.y,authoredColor(color),c.additive?hudAdditive:0);continue;}
            if(c.kind==OriginalControl::Kind::Button && !c.image){
                bool checked=c.checkedState || (item && item->selected),selectedFace=checked!=down;
                auto face=c;face.kind=OriginalControl::Kind::Text;face.font=selectedFace?"smallBoldFont":"smallFont";face.color=selectedFace?glm::vec4(1,1,0,1):glm::vec4(1);
                auto lo=glm::vec2(bounds),hi=lo+glm::vec2(bounds.z,bounds.w);if(clip){lo=glm::max(lo,glm::vec2(*clip));hi=glm::min(hi,glm::vec2(*clip)+glm::vec2(clip->z,clip->w));}
                if(hi.x>lo.x && hi.y>lo.y)h.rect(layout.origin.x+lo.x,layout.origin.y+lo.y,hi.x-lo.x,hi.y-lo.y,{0,0,0,1});
                text(face,face.text,clip?&*clip:nullptr,translation);continue;
            }
            if(!c.image)continue;
            auto image=c.image;glm::vec4 uv=c.uv;
            if(!c.animation.empty()) {
                auto& state=h.menuAnimations[c.image.get()];float dt=state.time?float(now-state.time)*secondsPerNs:0;state.time=now;
                state.value=std::clamp(state.value+(hover?1.f:-1.f)*dt/std::max(.001f,c.duration),0.f,1.f);
                image=c.animation[std::min(c.animation.size()-1,size_t(state.value*c.animation.size()))];
                for(auto& frame:c.animation)h.renderer.addTexture(frame,true);
            } else if(c.kind==OriginalControl::Kind::Button && !c.faces) {
                bool checked=c.checkedState || (item && item->selected);
                if(checked!=down && c.downImage)image=c.downImage;
            } else if(c.kind==OriginalControl::Kind::Button) {
                uint32_t face=1;auto choose=[&](uint32_t a,uint32_t b){return c.faces&a?a:b;};
                auto checked=c.checkedState?c.checkedState:item && item->selected?1:0;
                uint32_t base=1u<<(4*checked);
                // ImageButtonFacePane's exact sparse-strip fallbacks apply to
                // both checked states. An absent resulting face is not drawn.
                if(item && !item->enabled)face=checked?choose(base<<3,8):8;
                else if(down)face=choose(base<<1,1);
                else if(hover)face=checked?choose(base<<2,choose(4,choose(base,1))):choose(4,1);
                else if(checked)face=choose(base,2);
                if(!(c.faces&face))continue;
                auto frame=std::popcount(c.faces&(face-1)),count=std::popcount(c.faces);uv.y=float(frame)/count;uv.w=float(frame+1)/count;
            }
            auto lo=glm::vec2(bounds),hi=lo+glm::vec2(bounds.z,bounds.w);
            if(clip){auto oldLo=lo;lo=glm::max(lo,glm::vec2(*clip));hi=glm::min(hi,glm::vec2(*clip)+glm::vec2(clip->z,clip->w));auto span=glm::vec2(uv.z,uv.w)-glm::vec2(uv);auto lower=glm::vec2(uv)+(lo-oldLo)/glm::vec2(bounds.z,bounds.w)*span,upper=glm::vec2(uv)+(hi-oldLo)/glm::vec2(bounds.z,bounds.w)*span;uv={lower,upper};}
            if(hi.x>lo.x && hi.y>lo.y)h.rect(layout.origin.x+lo.x,layout.origin.y+lo.y,hi.x-lo.x,hi.y-lo.y,authoredColor(c.color),h.renderer.addTexture(image,true)|(c.additive?hudAdditive:0),uv);
        }
        if(menu.scroll){
            const auto& scroll=*menu.scroll;auto clip=scroll.bounds;
            h.rect(layout.origin.x+clip.x,layout.origin.y+clip.y,clip.z,clip.w,{0,0,0,1});
            HudValues values;values[HudNumber::UiTime]=float(std::max(0.,uiTime()-scroll.started));float offset=scroll.offset?-scroll.offset->evaluate(values):0;
            for(const auto& item:scroll.artwork)if(item.kind==OriginalControl::Kind::Text)text(item,item.text,&clip,{clip.x,clip.y+offset});
        }
        if(!menu.notice.empty()){OriginalControl c;c.bounds={20,580,760,20};c.font="smallFont";c.color={.878f,.573f,0,1};text(c,menu.notice);}
        return h.quads;
    }
    const auto& background=h.sprites.at("menuintroscreen/images/menuintroscreen_bg.jpg");
    float fit=std::max(h.extent.x/background.w,h.extent.y/background.h);
    h.rect((h.extent.x-background.w*fit)/2,(h.extent.y-background.h*fit)/2,background.w*fit,background.h*fit,{.45f,.45f,.45f,1},background.texture);
    glm::vec2 origin=menuLayout(size,h.userScale).origin;
    h.rect(origin.x,origin.y,1000,620,{.006f,.009f,.016f,.96f});
    h.rect(origin.x,origin.y,1000,2,gold);h.rect(origin.x+244,origin.y+84,1,510,{.15f,.19f,.24f,1});
    h.text(origin.x+24,origin.y+20,menu.title,gold,Title);
    h.text(origin.x+270,origin.y+25,menu.subtitle,white,Bold,0,684);
    auto paragraph=[&](const MenuText& line){
        Font font=line.heading?Bold:Normal;float x=origin.x+line.position.x,y=origin.y+line.position.y;
        std::string_view remaining=line.text;
        while(!remaining.empty()) {
            size_t end=remaining.find('\n');if(end==std::string_view::npos)end=remaining.size();
            std::string_view part=remaining.substr(0,end);
            if(h.width(part,font)>line.width) {
                size_t split=0;
                for(size_t i=0;i<part.size();) {size_t word=part.find(' ',i);if(word==std::string_view::npos)word=part.size();
                    if(h.width(part.substr(0,word),font)>line.width)break;split=word;i=word+1;}
                if(split)end=split;
            }
            h.text(x,y,remaining.substr(0,end),white,font,0,line.width);y+=22;
            remaining.remove_prefix(end);if(!remaining.empty())remaining.remove_prefix(1);
        }
    };
    for(auto& t:menu.text)paragraph(t);
    for(size_t i=0;i<menu.items.size();++i) {
        auto& item=menu.items[i];auto b=item.bounds;b.x+=origin.x;b.y+=origin.y;
        bool hover=item.enabled && inside(b,h.pointer),focus=item.enabled && int(i)==menu.focus;
        h.rect(b.x,b.y,b.z,b.w,hover?glm::vec4(.08f,.12f,.17f,1):glm::vec4(.02f,.035f,.055f,1));
        h.rect(b.x,b.y,b.z,1,item.selected || focus?gold:muted);
        glm::vec4 color=item.enabled?white:muted;
        float valueWidth=item.value.empty()?0:std::min(b.z*.45f,h.width(item.value,Small)+14);
        h.label({b.x+12,b.y,b.z-24-valueWidth,b.w},item.label,color,Normal);
        if(!item.value.empty())h.label({b.x+b.z-valueWidth-12,b.y,valueWidth,b.w},item.value,item.selected?gold:color,Small,1);
    }
    if(!menu.notice.empty()){h.rect(origin.x+256,origin.y+591,732,27,panel);h.label({origin.x+268,origin.y+591,708,27},menu.notice,gold,Compact);}
    return h.quads;
}
HudBackground Hud::buildBackground(glm::ivec2 size,const GameState& game,const CameraState& camera) {
    auto& h=*impl_;h.backgrounds.clear();
    bool first=h.starSector!=game.sectorId || h.starHistory.size()!=game.stars.size() || h.starSize!=size;
    h.starSector=game.sectorId;h.starSize=size;h.starHistory.resize(game.stars.size());
    auto right=glm::normalize(glm::cross(camera.forward,camera.up)),up=glm::cross(right,camera.forward);
    float focus=.5f*size.y/std::tan(glm::radians(camera.fieldOfView)*.5f);glm::vec2 center=glm::vec2(size)*.5f;
    auto ndc=[&](glm::vec2 p){return p*2.f/glm::vec2(size)-glm::vec2(1);};
    auto inside=[&](glm::vec2 p){return p.x>=0 && p.y>=0 && p.x<size.x && p.y<size.y;};
    for(size_t i=0;i<game.stars.size();++i) {
        auto& star=game.stars[i];Allegiance::StarPoint point;
        if(!Allegiance::ProjectPerspectiveStar(glm::dot(right,star.direction),glm::dot(up,star.direction),glm::dot(camera.forward,star.direction),focus,center.x,center.y,point)) {
            h.starHistory[i]={-1,-1};continue;
        }
        // Reentering the view starts a point, rather than drawing a streak from
        // an offscreen or behind-camera projection retained in the history.
        glm::vec2 p{point.x,point.y},old=first || !inside(h.starHistory[i])?p:h.starHistory[i];h.starHistory[i]=p;
        auto motion=Allegiance::StarMotion(point,{old.x,old.y});float brightness=star.brightness*motion.brightness;
        if(motion.point)h.backgrounds.push_back({{ndc(p),ndc(p+glm::vec2(1))},{0,0,1,1},{brightness,brightness,brightness,1},0});
        else {
            glm::vec2 end{motion.end.x,motion.end.y};if(!inside(end))continue;
            glm::vec2 d=end-p,normal=glm::vec2(-d.y,d.x)/glm::length(d)/glm::vec2(size);
            h.backgrounds.push_back({{ndc(p+glm::vec2(.5f)),ndc(end+glm::vec2(.5f))},{normal,0,0},{brightness,brightness,brightness,1},hudLine|hudAdditive});
        }
    }
    uint32_t posterOffset=uint32_t(h.backgrounds.size());
    // WinTrek's flare renderer uses the fixed +Z direction, independent of sector lighting.
    glm::vec3 light{0,0,1};float depth=glm::dot(camera.forward,light);
    if(h.visual.lensFlare && !game.commandView && depth>0) {
        glm::vec2 relative{focus*glm::dot(right,light)/depth,-focus*glm::dot(up,light)/depth};
        if(inside(center+relative)) {
            constexpr float positions[]={-1,-.75f,-.5f,-.25f,.25f,.5f,.75f,1},scales[]={1.5f,1,.5f,.25f,.25f,.5f,1,1.5f};
            for(int i=0;i<8;++i) {
                int image=i==0?2:i==7?1:0;auto& sprite=h.sprites.at(image==0?"f104bmp":image==1?"f105bmp":"f106bmp");
                float scale=float(size.x)/800*scales[i],angle=image==1?(relative.x+relative.y)/200:0;
                glm::vec2 axis{std::cos(angle),std::sin(angle)},normal{-axis.y,axis.x},p=center+relative*positions[i];
                glm::vec2 half=axis*(sprite.w*scale*.5f),height=normal*(sprite.h*scale)/glm::vec2(size);
                h.backgrounds.push_back({{ndc(p-half),ndc(p+half)},{height,0,0},image==0?glm::vec4(0,0,1,1):glm::vec4(1),sprite.texture|hudAdditive|hudOriented});
            }
        }
    }
    return {h.backgrounds,posterOffset};
}
std::span<const HudQuad> Hud::build(glm::ivec2 size,const GameState& g,std::span<const Contact> contacts,
    const glm::mat4& vp,const RenderStats& stats,bool paused,bool help,bool cursor,glm::vec2 pointer,const CommandGesture* gesture,const CameraState* camera,std::span<const int> investmentSectors,uint32_t consoleOverlays) {
    auto& h=*impl_;h.resize(size);h.mapActive=false;h.inventoryBounds={};h.commandBounds={};h.chatBounds={};h.quads.clear();h.hits.clear();h.hitScale=h.scale;h.preview.reset();h.cursor=cursor;h.pointer=pointer/h.scale;h.updateCounters(g);
    if(g.hull<h.lastHull)h.hullFlash=g.time+0.2;if(g.shield<h.lastShield)h.shieldFlash=g.time+0.2;h.lastHull=g.hull;h.lastShield=g.shield;
    if(g.trainingMission){
        if(!h.trainingOverride){h.savedInventory=h.inventory;h.savedMap=h.map;h.savedRadar=h.radarMode;h.trainingOverride=true;}
        h.inventory=g.trainingPanes&ofInventory;h.map=g.trainingPanes&ofSectorPane;h.radarMode=g.trainingRadar;
    } else if(h.trainingOverride){h.inventory=h.savedInventory;h.map=h.savedMap;h.radarMode=h.savedRadar;h.trainingOverride=false;}
    h.commandRadarView=g.commandView;
    if(!g.trainingMission)h.radarMode=g.commandView?h.commandRadar:(g.camera==CameraMode::Cockpit || g.camera==CameraMode::Chase)?h.cockpitRadar:Allegiance::c_rlNone;
    h.chatGeometry(consoleOverlays);
    if(h.visual.messages){h.layoutMessages(g);h.messagePager.update(h.messageLines,uiTime());}
    const auto headers=h.headers(g);
    if(h.modern && g.cinematic && !paused && !help){if(h.visual.messages)h.messages(g,g.commandView?72.f:36.f);return h.quads;}
    if(h.visible) {
        if(!g.dead && (!g.docked || g.commandView)){h.radar(g,contacts,vp);if(!g.commandView && h.visual.centerHud)h.center(g);if(h.visual.targetHud)h.targetPanel(g);if(!g.commandView)h.steering(g);if(h.modern)h.queuedRequest(g);}
        if(h.modern){h.sectorMap(g,investmentSectors,consoleOverlays);h.inventoryPanel(g);}h.contactPanel(contacts);
        if(g.commandView && h.modern) {
            const float x=h.extent.x-264,y=64;
            h.rect(x,y,232,250,panel);
            char group[64];snprintf(group,sizeof(group),"%d ships selected",g.commandUnitCount);
            h.label({x+8,y+8,216,24},g.commandUnitCount>1?std::string_view(group):g.commandUnitName.empty()?"Select a friendly ship":std::string_view(g.commandUnitName),white,Small);
            constexpr const char* names[]={"Default","Attack","Capture","Defend","Pick up","Go to","Repair","Join","Mine","Build"};
            for(int i=0;i<10;++i)h.button({x+8+(i%2)*110.f,y+40+(i/2)*30.f,106,26},names[i],{HudCommand::CommandOrder,i},g.commandOrder==i,Compact);
            h.button({x+8,y+200,216,30},h.teamKeys,{HudCommand::Team},false,Compact);
            h.label({h.extent.x*.5f-330,44,660,24},"COMMAND VIEW  /  Drag: select  /  Shift-click: group  /  Right-drag: height",white,Small,.5f);
        }


        if(g.commandView){auto north=vp*glm::vec4(0,5625,0,1);if(north.w>0){auto p=(glm::vec2(north)/north.w+1.f)*.5f*h.extent;h.text(p.x,p.y,"NORTH",white,Bold,.5f);}}
        if(gesture) {
            if(gesture->selectionStart && Allegiance::CommandSelectionRectangle(gesture->pointer.x-gesture->selectionStart->x,gesture->pointer.y-gesture->selectionStart->y)) {
                auto low=glm::min(*gesture->selectionStart,gesture->pointer)/h.scale,high=glm::max(*gesture->selectionStart,gesture->pointer)/h.scale;
                h.rect(low.x,low.y,high.x-low.x,high.y-low.y,{.2f,.8f,1,.08f});
                h.line(low,{high.x,low.y},1,white);h.line({high.x,low.y},high,1,white);
                h.line(high,{low.x,high.y},1,white);h.line({low.x,high.y},low,1,white);
            }
            if(gesture->orderStart && camera)if(auto destination=gesture->destination(size,vp,*camera)) {
                auto a=vp*glm::vec4(destination->x,destination->y,0,1),b=vp*glm::vec4(*destination,1);
                if(a.w>0 && b.w>0) {
                    auto p=(glm::vec2(a)/a.w+1.f)*.5f*h.extent,q=(glm::vec2(b)/b.w+1.f)*.5f*h.extent;
                    auto color=destination->z>0?glm::vec4(200.f/255,180.f/255,20.f/255,1):glm::vec4(150.f/255,100.f/255,0,1);h.line(p,q,2,color);
                    h.line(p-glm::vec2(7,0),p+glm::vec2(7,0),1,color);h.line(p-glm::vec2(0,7),p+glm::vec2(0,7),1,color);
                    h.line(q-glm::vec2(5,0),q+glm::vec2(5,0),2,color);h.line(q-glm::vec2(0,5),q+glm::vec2(0,5),2,color);
                    char height[64];snprintf(height,sizeof(height),"Height %+.0f m",destination->z);
                    h.text(std::clamp(q.x+12,8.f,h.extent.x-140),std::clamp(q.y-20,8.f,h.extent.y-28),height,color,Small);
                }
            }
        }

        if(!h.modern)h.authoredStatus(g,consoleOverlays,investmentSectors);
        if(h.modern && g.sideActive && g.ghost)h.label({h.extent.x/2-310,headers.ghost,620,24},"NO LIVES REMAIN  /  Command view",gold,Small,.5f);
        if(h.modern && !g.carriedObjective.empty())h.label({h.extent.x/2-310,headers.carried,620,24},g.carriedObjective,gold,Small,.5f);
        if(h.modern && g.sideActive && !g.objective.empty())h.label({h.extent.x/2-310,headers.objective,620,24},g.objective,gold,Small,.5f);
        if(h.modern){h.text(36,h.extent.y/2,h.counterText,white,Small);
            h.text(36,18,g.sessionName,muted,Small);h.text(h.extent.x-36,18,g.sector,white,Bold,1);
            h.text(h.extent.x/2,h.extent.y-20,cursor?h.cursorKeys:h.flightKeys,muted,Small,0.5f);
            if(h.radarMode!=1)h.text(36,43,h.radarLabels[h.radarMode+1],muted,Small);}
    }
    if(h.visible && h.visual.messages)h.messages(g,headers.messages);
    if(g.trainingMission && g.trainingOverlay!=NoTrainingOverlay && !g.commandView && (g.camera==CameraMode::Cockpit || g.camera==CameraMode::Chase || g.camera==CameraMode::Missile)) {
        const auto callout=Allegiance::TrainingCalloutFor(TrainingOverlay(g.trainingOverlay),0,0,h.extent.x,h.extent.y);
        auto font=h.assets.font("smallFont");float width=0;
        for(const unsigned char* p=reinterpret_cast<const unsigned char*>(callout.description);*p;++p)width+=font->glyphs[*p].z;
        const auto text=Allegiance::TrainingTextFor(callout,width,float(font->height));
        auto point=[&](float x,float y){return glm::vec2(x,h.extent.y-y);};
        const glm::vec4 color{1,0,0,.9f};
        h.line(point(callout.ax,callout.ay),point(callout.bx,callout.by),2,color);
        h.line(point(callout.bx,callout.by),point(text.cx,text.cy),2,color);
        // Context::DrawString treats the upward point as the text's top.
        h.bitmapText("smallFont",text.x,h.extent.y-text.y,callout.description,color,h.extent.x);
        if(*callout.secondary)h.bitmapText("smallFont",text.secondaryX,h.extent.y-text.secondaryY,callout.secondary,color,h.extent.x);
        const auto& dot=h.sprites.at("tm_hud_overlaybmp");
        h.image("tm_hud_overlaybmp",callout.ax-std::floor(dot.w*.5f),h.extent.y-callout.ay-std::floor(dot.h*.5f),glm::vec4(1),false);
    }
    if(g.trainingPaused)h.label({h.extent.x*.5f-220,headers.paused,440,28},"Training paused / Pause to resume",gold,Normal,.5f);
    if(h.diagnostics) {
        char label[160];snprintf(label,sizeof(label),"Refresh %.2f Hz  latency %.2f ms  %u draws  %zu UI quads  %llu misses",stats.refreshHz,stats.latencyMs,stats.draws,h.quads.size(),(unsigned long long)stats.missed);
        h.text(h.extent.x/2,h.extent.y-42,label,muted,Small,0.5f);
    }
    if(h.modern && (paused || help || g.dead || (g.docked && !g.commandView && !g.trainingCommand))){h.hits.clear();h.modal(g,paused,help);}
    return h.quads;
}
std::span<const HudQuad> Hud::buildSectorMap(glm::ivec2 size,const GameState& g,glm::vec2 pointer,uint32_t consoleOverlays,std::span<const int> investmentSectors) {
    auto& h=*impl_;h.resize(size);h.mapActive=false;h.cursor=true;h.pointer=pointer/h.scale;
    if(!h.modern)h.authoredGeometry(g,consoleOverlays);h.sectorMap(g,investmentSectors,consoleOverlays);return h.quads;
}
std::span<const HudQuad> Hud::appendMessages(glm::ivec2 size,const GameState& g,uint32_t consoleOverlays) {
    auto& h=*impl_;h.resize(size);
    h.chatGeometry(consoleOverlays);
    if(h.visible && h.visual.messages){h.layoutMessages(g);h.messagePager.update(h.messageLines,uiTime());h.messages(g,36.f);}
    return h.quads;
}
std::span<const HudQuad> Hud::appendStatus(glm::ivec2 size,const GameState& g,uint32_t consoleOverlays) {
    auto& h=*impl_;h.resize(size);h.authoredStatus(g,consoleOverlays);return h.quads;
}
}
