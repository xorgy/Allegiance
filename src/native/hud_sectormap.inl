// Members of Hud::Impl. Original sector images remain resident in the existing
// bindless HUD batch; this adapter contains no legacy Image/Pane objects.
bool mapExpanded{true},mapDragging{},mapActive{};
float mapHitScale{1};glm::vec4 mapViewport{},mapExpandBounds{},mapPanelBounds{};glm::vec2 mapLastPointer{};
Allegiance::SectorMapPoint mapDrag{};Allegiance::SectorMapBounds mapBounds;
std::vector<Allegiance::SectorMapPoint> mapPositions;
struct MapNode {int id;glm::vec2 point;};std::vector<MapNode> mapNodes;
std::function<const SectorInfo&(int)> sectorInfoService;
std::shared_ptr<const BitmapFont> mapFont;uint32_t mapFontTexture{};
bool mapShowing{true};double mapSlideStarted{};float mapSlideFrom{1},mapSlideTo{1};
std::array<glm::vec4,6> mapSourceColors{},mapColors{};
int mapSectorAt(glm::vec2 p)const {
    int id=-1,nearest=15;
    for(auto& node:mapNodes){int distance=int(glm::length(p-node.point));if(distance<nearest){nearest=distance;id=node.id;}}
    return id;
}
void mapRect(glm::vec4 bounds,glm::vec4 clip,glm::vec4 color,uint32_t texture=0,glm::vec4 uv={0,0,1,1}) {
    auto lo=glm::max(glm::vec2(bounds),glm::vec2(clip)),hi=glm::min(glm::vec2(bounds)+glm::vec2(bounds.z,bounds.w),glm::vec2(clip)+glm::vec2(clip.z,clip.w));
    if(hi.x<=lo.x || hi.y<=lo.y)return;
    auto span=glm::vec2(uv.z,uv.w)-glm::vec2(uv),begin=glm::vec2(uv)+(lo-glm::vec2(bounds))/glm::vec2(bounds.z,bounds.w)*span,end=glm::vec2(uv)+(hi-glm::vec2(bounds))/glm::vec2(bounds.z,bounds.w)*span;
    rect(lo.x,lo.y,hi.x-lo.x,hi.y-lo.y,color,texture,{begin,end});
}
const Sprite& mapSprite(std::string_view name) {
    auto found=sprites.find(name);if(found!=sprites.end())return found->second;
    auto source=assets.image(std::string(name),true);return sprites.emplace(std::string(name),Sprite{renderer.addTexture(source,true),float(source->width),float(source->height)}).first->second;
}
void mapImage(std::string_view name,glm::vec2 p,glm::vec4 clip,glm::vec4 color=glm::vec4(1),bool centered=false) {
    auto sprite=mapSprite(name);if(centered)p-=glm::floor(glm::vec2(sprite.w,sprite.h)*.5f);
    mapRect({p,sprite.w,sprite.h},clip,color,sprite.texture);
}
float mapText(glm::vec2 p,std::string_view text,glm::vec4 clip) {
    if(!mapFont){mapFont=assets.font("smallFont");mapFontTexture=renderer.addTexture(mapFont->coverage,true);}
    float x=std::floor(p.x),y=std::floor(p.y),begin=x;
    for(unsigned char ch:text){auto g=mapFont->glyphs[ch];if(g.z>0 && g.w>0)mapRect({x,y,g.z,g.w},clip,glm::vec4(1),mapFontTexture,{g.x/mapFont->coverage->width,g.y/mapFont->coverage->height,(g.x+g.z)/mapFont->coverage->width,(g.y+g.w)/mapFont->coverage->height});x+=g.z;}
    return x-begin;
}
void mapLine(glm::vec2 a,glm::vec2 b,glm::vec4 clip) {
    float start=0,end=1;auto d=b-a;
    auto edge=[&](float p,float q){if(std::abs(p)<1e-10f)return q>=0;float t=q/p;if(p<0)start=std::max(start,t);else end=std::min(end,t);return start<=end;};
    if(edge(-d.x,a.x-clip.x) && edge(d.x,clip.x+clip.z-a.x) && edge(-d.y,a.y-clip.y) && edge(d.y,clip.y+clip.w-a.y))line(a+d*start,a+d*end,1,{.214041f,.214041f,.214041f,1});
}
void sectorMap(const GameState& g,std::span<const int> investmentSectors,uint32_t consoleOverlays=0) {
    glm::vec2 origin;bool showing;
    if(modern) {
        double now=uiTime();float progress=std::clamp(float((now-mapSlideStarted)/.5),0.f,1.f);
        float shown=mapSlideFrom+(mapSlideTo-mapSlideFrom)*progress;
        showing=map || (consoleOverlays&(ofInvestment|ofTeleportPane));
        if(showing!=mapShowing){mapShowing=showing;mapSlideStarted=now;mapSlideFrom=shown;mapSlideTo=showing?1:0;}
        if(!showing && shown<=0)return;
        origin={49,extent.y+46-223*shown};
    } else {
        auto index=mapOverlay[hudStyle];if(index==size_t(-1))return;
        const auto& spec=overlayRecords[index];showing=(overlayFlags&spec.mask)!=0;
        auto& motion=overlayMotions[hudStyle][index];if(!motion.update(showing,overlayTime,spec.duration))return;
        origin=glm::floor(motion.point(spec,overlayTime))-glm::vec2(0,141);
        showing=showing && !spec.undetectable;
    }
    if(!showing)mapDragging=false;
    mapActive=showing;mapHitScale=scale;float panelWidth=mapExpanded?211:151;
    glm::vec4 infoClip{origin,panelWidth,141};mapPanelBounds=infoClip;
    image(mapExpanded?"CortUI/classic/minimappanelexpanded.png":"CortUI/classic/minimappanel.png",origin.x,origin.y,glm::vec4(1),false);
    image("CortUI/classic/sectormapbg.png",origin.x+9,origin.y+25,glm::vec4(1),false);
    image("CortUI/classic/minimapexpandbtn.png",origin.x+(mapExpanded?170:110),origin.y+5,glm::vec4(1),false);
    auto button=mapSprite("consectorexpandbmp");mapExpandBounds={origin.x+panelWidth-button.w-16,origin.y,panelWidth-(panelWidth-button.w-16),button.h+4};
    mapViewport={origin+glm::vec2(9,25),120,96};auto clip=glm::vec4(glm::vec2(mapViewport)+glm::vec2(1),mapViewport.z-2,mapViewport.w-2);
    mapPositions.clear();for(auto& s:g.sectors)if(s.known)mapPositions.push_back({s.position.x,s.position.y});
    mapBounds=Allegiance::SectorMapBounds::Calculate(mapPositions,mapViewport.z,mapViewport.w,mapDrag);mapDrag=mapBounds.drag;
    mapNodes.clear();for(auto& s:g.sectors)if(s.known){auto p=mapBounds.Pixel({s.position.x,s.position.y},mapViewport.z,mapViewport.w);mapNodes.push_back({s.id,glm::vec2(mapViewport)+glm::vec2(p.x,p.y)});}
    auto find=[&](int id)->const MapNode*{auto p=std::find_if(mapNodes.begin(),mapNodes.end(),[&](auto& p){return p.id==id;});return p==mapNodes.end()?nullptr:&*p;};
    for(auto link:g.sectorLinks){auto* a=find(link.from);auto* b=find(link.to);if(a && b)mapLine(a->point,b->point,clip);}
    if(mapSourceColors!=g.sideColors){mapSourceColors=g.sideColors;mapColors=g.sideColors;for(auto& color:mapColors)for(int c=0;c<3;++c)color[c]=color[c]<=.04045f?color[c]/12.92f:std::pow((color[c]+.055f)/1.055f,2.4f);}
    auto sideColor=[&](int side){return side>=0 && side<int(mapColors.size())?mapColors[side]:glm::vec4(1);};
    int hovered=cursor && inside(mapViewport,pointer) && !mapDragging?mapSectorAt(pointer):-1;
    bool flash=(uint64_t(uiTime()*10)%4)>=2; // SectorMapPane toggles every two 0.1s ticks.
    const SectorState* selected=nullptr;
    for(auto& s:g.sectors)if(auto* node=find(s.id)){
        auto p=node->point;mapImage("sectoremptybmp",p,clip,s.highlight && !flash?glm::vec4(0,1,1,1):glm::vec4(1),true);
        if(auto warning=Allegiance::SectorWarningImage(s.warning);warning && flash)mapImage(warning,p,clip,glm::vec4(1),true);
        if(s.current)mapImage("sectorhighlightbmp",p,clip,glm::vec4(1),true);
        else if(s.id==hovered)mapImage("sectorselbmp",p,clip,glm::vec4(1),true);
        if(s.accepted)mapImage("sectortargetbmp",p,clip,glm::vec4(1),true);
        else if(s.queued)mapImage("sectorqueuedbmp",p,clip,glm::vec4(1),true);
        if(consoleOverlays&ofTeleportPane){if(s.restartStation>=0 && !s.current)mapImage("sectorripcordbmp",p,clip,glm::vec4(1),true);}
        else if(g.ripcord){
            if(s.id==g.ripcordSector)mapImage(g.ripcordShip?(flash?"sectorripcordtargetbmp":"sectorripcordbmp"):(flash?"sectorripcordstationtargetbmp":"sectorripcordstationbmp"),p,clip,glm::vec4(1),true);
            else if(s.ripcordKind)mapImage(s.ripcordKind==1?"sectorripcordstationbmp":"sectorripcordbmp",p,clip,glm::vec4(1),true);
        }else if(std::binary_search(investmentSectors.begin(),investmentSectors.end(),s.id))mapImage("sectorripcordbmp",p,clip,glm::vec4(1),true);
        if(s.owner>=0)mapImage("sectorownerbmp",p,clip,sideColor(s.owner),true);
        if(s.secondaryOwner>=0)mapImage("sectorinvaderbmp",p,clip,sideColor(s.secondaryOwner),true);
        if(s.highlight && !flash)mapImage("sectorinvaderbmp",p,clip,{0,1,1,1},true);
        int present=std::count_if(s.population.begin(),s.population.end(),[](int n){return n>0;});
        float y=p.y+float((present*3-1)/2)-1; // Original bottom-origin row order.
        for(size_t side=0;side<s.population.size();++side)if(int count=std::min(7,s.population[side]);count>0){
            float x=p.x+6;while(count>0){int dots=std::min(2,count);mapRect({x,y,dots,2},clip,sideColor(int(side)));count-=dots;x+=3;}y-=3;
        }
        if(s.id==hovered || (hovered<0 && s.current))selected=&s;
    }
    if(!selected)return;
    if(!mapFont){mapFont=assets.font("smallFont");mapFontTexture=renderer.addTexture(mapFont->coverage,true);}
    auto textBottom=[&](float x,float y,std::string_view value){return mapText(origin+glm::vec2(x,141-y-mapFont->height),value,infoClip);};
    textBottom(24,128,selected->name+" Sector");
    if(sectorInfoService){const auto& info=sectorInfoService(selected->id);float x=4;
        for(auto& icon:info.assets)if(x<mapExpandBounds.x-origin.x){auto sprite=mapSprite(icon.image);mapImage(icon.image,origin+glm::vec2(x,141-115-sprite.h),infoClip,sideColor(icon.side));x+=sprite.w;
            if(icon.count>1){char number[20];snprintf(number,sizeof(number),"%d",icon.count);x+=textBottom(x,115+(sprite.h-mapFont->height)*.5f,number)+2;}}
        if(mapExpanded){char helium[40];snprintf(helium,sizeof(helium),"He3: %d",int(info.helium));textBottom(140,96,helium);float y=81;
            for(auto& rock:info.asteroids){auto sprite=mapSprite(rock.image);mapImage(rock.image,origin+glm::vec2(140,141-y-sprite.h),infoClip);char number[20];snprintf(number,sizeof(number),"%d",rock.count);textBottom(144+sprite.w,y,number);y-=sprite.h;}
            auto probe=mapSprite("probebmp");mapImage("probebmp",origin+glm::vec2(140,141-y-probe.h),infoClip,sideColor(g.playerSide));char number[20];snprintf(number,sizeof(number),"%d",info.probes);textBottom(144+probe.w,y,number);
        }
    }
    if(hovered>=0 && selected->warning>0)textBottom(0,0,GetClusterWarningText(selected->warning));
}
