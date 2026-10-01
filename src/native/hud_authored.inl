// Members of Hud::Impl. All authored styles are flattened and made resident at
// load time. The frame path reads compact client values and appends native quads.
enum AuthoredRoot {CenterHud,SoftwareHud,ParciHud,TargetHud,LargeTarget,DetectedHud,CommandIcons,CommandInfo,BaseInfo,PodHud,DeathHud,RipcordHud,LivesHud,KillsHud,CountdownHud,FlightLocation,StationLocation,ViewLocation,RootCount};
struct ResidentFont {std::shared_ptr<const BitmapFont> font;uint32_t texture{};};
struct ResidentHud {std::unique_ptr<HudProgram> program;std::vector<ResidentFont> fonts;};
std::array<std::array<ResidentHud,RootCount>,6> authored;
std::unordered_map<std::string,std::array<ResidentHud,6>> authoredInventory;
std::vector<std::pair<HudFont,ResidentFont>> authoredFonts;
std::unordered_map<const ImageData*,uint32_t> authoredTextures;
int hudStyle{};
std::optional<ModelPreview> secondPreview;
std::array<ResidentHud,6> inventoryShell;
std::array<std::unique_ptr<HudOverlayTable>,6> overlayTables;
std::array<std::vector<int>,6> overlayRoots;
std::array<std::vector<HudOverlayMotion>,6> overlayMotions;
std::array<size_t,6> mapOverlay;
std::span<const HudOverlay> overlayRecords;
uint32_t overlayFlags{};
double overlayTime{};
uint32_t authoredFlags(const GameState& g,uint32_t overlays)const {
    uint32_t flags=originalOverlayModes(g,overlays);
    if(inventory && !g.dead && !g.docked)flags|=ofInventory;
    if(map)flags|=ofSectorPane;
    return flags;
}
HudValues authoredGeometry(const GameState& g,uint32_t overlays) {
    overlayFlags=authoredFlags(g,overlays);overlayTime=uiTime();auto values=authoredValues(g);
    float height=inventoryColumnTop.h+inventoryColumnBottom.h,width=std::max(inventoryColumnTop.w,inventoryColumnBottom.w);
    for(const auto& row:g.inventory){auto found=authoredInventory.find(row.mdl);if(found==authoredInventory.end())throw std::runtime_error("Inventory MDL was not preloaded: "+row.mdl);
        auto size=found->second[hudStyle].program->paneSize();height+=size.y;width=std::max(width,size.x);}
    values[HudNumber::InventoryHeight]=height;values[HudNumber::InventoryWidth]=width;
    overlayRecords=overlayTables[hudStyle]->evaluate(values,extent);return values;
}
Sprite inventoryColumnTop,inventoryColumnBottom;
void preloadInventory(OriginalUi& ui,std::string_view source) {
    std::string name(source);for(char& ch:name)if(ch>='A' && ch<='Z')ch+=char('a'-'A');
    if(authoredInventory.contains(name))return;
    std::array<ResidentHud,6> styles;
    for(int style=0;style<6;++style) {
        auto& item=styles[style];item.program=std::make_unique<HudProgram>(ui,"InventoryLine",style,name,true);
        for(auto& spec:item.program->fonts())item.fonts.push_back(authoredFont(spec));
        for(auto& image:item.program->images())authoredTextures.emplace(image.get(),renderer.addTexture(image,true));
    }
    authoredInventory.emplace(std::move(name),std::move(styles));
}
HudValues authoredValues(const GameState& g)const {
    HudValues v;using N=HudNumber;using S=HudString;using C=HudColor;
    v[N::ShowCenter]=visual.centerHud;v[N::ShowTarget]=visual.targetHud;
    v[N::Hull]=g.hull;v[N::Shield]=g.shield;v[N::Energy]=g.energy;v[N::Fuel]=g.fuel;v[N::Ammo]=g.ammoFraction;v[N::Throttle]=g.throttle;
    v[N::Speed]=g.speed;v[N::Experience]=g.experience;v[N::Signature]=g.signature;v[N::Flash]=g.time<hullFlash || g.time<shieldFlash;
    v[N::Artifact]=g.carriedFlag==SIDE_TEAMLOBBY;v[N::Flag]=g.carriedFlag>=0;v[N::Detected]=g.detected;v[N::VectorLock]=g.coast;
    v[N::Autopilot]=g.autopilot;v[N::Cloak]=g.cloaked;v[N::Cloaking]=g.cloakingPercent;v[N::Pod]=g.pod;v[N::Endurance]=g.endurance;
    v[N::Ripcord]=g.ripcord;v[N::RipcordLeft]=g.ripcordLeft;v[N::TargetHull]=g.targetHull;v[N::TargetShield]=g.targetShield;
    v[N::TargetSpeed]=g.targetSpeed;v[N::TargetRange]=g.target.empty()?-1:g.targetDistance;v[N::TargetExperience]=g.targetExperience;
    v[N::TargetType]=g.targetObjectType;v[N::TargetExists]=!g.target.empty();v[N::InRange]=g.inRange;
    v[N::StationHull]=g.stationHull;v[N::StationShield]=g.stationShield;v[N::Lives]=g.lives;v[N::LivesOn]=g.lives<10;
    v[N::Kills]=g.kills;v[N::KillsOn]=g.killCounter;v[N::CountdownOn]=0; // WinTrek leaves this authored toggle at zero.
    v[N::BoardState]=g.passenger?(g.turret<0?1:2):0;v[N::Ghost]=g.ghost;v[N::ChatLines]=chatLines;
    v[S::Ship]=g.ship;v[S::Player]=g.sourcePilot;v[S::Target]=g.target;v[S::TargetType]=g.targetType;v[S::TargetSector]=g.targetSector;
    v[S::Station]=g.station;v[S::StationSector]=g.stationSector;v[S::Sector]=g.sector;
    auto side=g.playerSide>=0 && g.playerSide<int(g.sideColors.size())?g.sideColors[g.playerSide]:glm::vec4(1);
    v[C::Side]=side;v[C::TargetSide]=g.targetColor;v[C::Station]=g.stationColor;
    v[C::Player]=side*.85f;v[C::Player].a=.75f;
    v[C::Target]=g.targetColor*.85f;v[C::Target].a=.5f;
    if(g.targetSide<0)v[C::Target]={.5f,.5f,.5f,.5f};
    constexpr auto shadow=Allegiance::HudShadowArgb;
    v[C::Shadow]={float((shadow>>16)&255)/255,float((shadow>>8)&255)/255,float(shadow&255)/255,float(shadow>>24)/255};return v;
}
ResidentFont authoredFont(const HudFont& spec) {
    for(auto& cached:authoredFonts)if(cached.first==spec)return cached.second;
    ResidentFont resident;
    resident.font=assets.font(spec.symbol,spec.height,spec.bold);
    resident.texture=renderer.addTexture(resident.font->coverage,true);authoredFonts.push_back({spec,resident});return resident;
}
void preloadAuthoredHud(OriginalUi& ui) {
    constexpr const char* roots[]={"CenterHudDisplayImage","SoftCenterHudDisplayImage","SoftCenterHudDisplayParciStyleImage","TargetDisplayImage","GHLargeTargetView","GHMsgDetectedView","CommandModeIcons","cortCommandModeInfo","cortCommandModeInfoBase","EjectPodImage","DeathBannerPaneImage","RipcordCountdownImage","LivesImage","KillsImage","CountdownImage","LocationInCockpitImage","LocationInHangarImage","LookingAtImage"};
    for(int style=0;style<6;++style)for(int root=0;root<RootCount;++root){auto& item=authored[style][root];item.program=std::make_unique<HudProgram>(ui,roots[root],style);
        for(auto& spec:item.program->fonts())item.fonts.push_back(authoredFont(spec));
        for(auto& image:item.program->images())authoredTextures.emplace(image.get(),renderer.addTexture(image,true));}
    for(int style=0;style<6;++style) {
        overlayTables[style]=std::make_unique<HudOverlayTable>(ui,style);
        auto entries=overlayTables[style]->evaluate({},{});
        overlayRoots[style].assign(entries.size(),-1);overlayMotions[style].resize(entries.size());
        for(int root=0;root<RootCount;++root) {
            auto index=overlayTables[style]->find(roots[root]);if(index!=size_t(-1))overlayRoots[style][index]=root;
        }
        auto inventory=overlayTables[style]->find("MergedInventoryPaneImage");
        if(inventory!=size_t(-1))overlayRoots[style][inventory]=RootCount;
        mapOverlay[style]=overlayTables[style]->find("SectorImage");
        if(mapOverlay[style]!=size_t(-1))overlayRoots[style][mapOverlay[style]]=RootCount+1;
        auto command=overlayTables[style]->find("CommandPaneImage");
        if(command!=size_t(-1))overlayRoots[style][command]=RootCount+2;
    }
    // Style-dependent panel textures remain in the same bindless atlas/table.
    for(auto dir:{"classic","greytones","glasshud"})for(auto name:{"inventorytop.png","inventorylinebkgnd.png","inventorybottom.png","inventorytab.png","mountingbar.png","armingbar.png","minimappanelexpanded.png","minimappanel.png","minimapexpandbtn.png","sectormapbg.png"}) {
        std::string key=std::string("CortUI/")+dir+"/"+name;if(sprites.contains(key))continue;
        auto image=assets.image(key,true);sprites.emplace(key,Sprite{renderer.addTexture(image,true),float(image->width),float(image->height)});
    }
    for(auto name:{"mountingbarbkgndbmp","armingbarbkgndbmp","invequipmenticonbmp","invweaponiconbmp","misslebmp"}) {
        auto image=assets.image(name,true);sprites.emplace(name,Sprite{renderer.addTexture(image,true),float(image->width),float(image->height)});
    }
    for(int style=0;style<6;++style) {
        auto& shell=inventoryShell[style];shell.program=std::make_unique<HudProgram>(ui,"MergedInventoryPaneImage",style);
        for(auto& spec:shell.program->fonts())shell.fonts.push_back(authoredFont(spec));
        for(auto& image:shell.program->images())authoredTextures.emplace(image.get(),renderer.addTexture(image,true));
    }
    auto cap=[&](const char* name){auto image=assets.image(name,true);bool visible=false;for(size_t i=3;i<image->pixels.size();i+=4)visible|=image->pixels[i]!=0;
        return Sprite{visible?renderer.addTexture(image,true):0,float(image->width),float(image->height)};};
    inventoryColumnTop=cap("inventorytopbmp");inventoryColumnBottom=cap("inventorybottombmp");
    // Empty/crew rows are supplied by InventoryImage rather than part types.
    // Resident stock rows also support diagnostics before a core is loaded;
    // Game supplies every additional core-selected namespace at session entry.
    for(auto name:{"invemptyweapon","invemptymissile","invemptychaff","invemptyshield","invemptycloak","invemptyafterburner","invemptymine","invemptycargo","invobserver","invturret",
        "invdefault","invsweapon","invsmissile","invsmine","invchaff","invshield","invcloak","invafterburn"})preloadInventory(ui,name);
}
std::string_view hudPanel(std::string_view classic)const {
    // Call sites retain immutable literal paths; no string construction per frame.
    struct Panel {std::string_view classic,grey,glass;};
    static constexpr Panel panels[]={
        {"CortUI/classic/inventorytop.png","CortUI/greytones/inventorytop.png","CortUI/glasshud/inventorytop.png"},
        {"CortUI/classic/inventorylinebkgnd.png","CortUI/greytones/inventorylinebkgnd.png","CortUI/glasshud/inventorylinebkgnd.png"},
        {"CortUI/classic/inventorybottom.png","CortUI/greytones/inventorybottom.png","CortUI/glasshud/inventorybottom.png"},
        {"CortUI/classic/inventorytab.png","CortUI/greytones/inventorytab.png","CortUI/glasshud/inventorytab.png"},
        {"CortUI/classic/mountingbar.png","CortUI/greytones/mountingbar.png","CortUI/glasshud/mountingbar.png"},
        {"CortUI/classic/armingbar.png","CortUI/greytones/armingbar.png","CortUI/glasshud/armingbar.png"},
        {"CortUI/classic/minimappanelexpanded.png","CortUI/greytones/minimappanelexpanded.png","CortUI/glasshud/minimappanelexpanded.png"},
        {"CortUI/classic/minimappanel.png","CortUI/greytones/minimappanel.png","CortUI/glasshud/minimappanel.png"},
        {"CortUI/classic/minimapexpandbtn.png","CortUI/greytones/minimapexpandbtn.png","CortUI/glasshud/minimapexpandbtn.png"},
        {"CortUI/classic/sectormapbg.png","CortUI/greytones/sectormapbg.png","CortUI/glasshud/sectormapbg.png"}};
    if(modern || hudStyle==0 || hudStyle==1 || hudStyle==4)return classic;
    for(auto& panel:panels)if(panel.classic==classic)return hudStyle==3?panel.glass:panel.grey;return classic;
}
void authoredText(const HudDraw& d,const ResidentFont& resident,glm::vec2 origin,std::string_view value={}) {
    const auto& font=*resident.font;auto measure=[&](std::string_view text){return Allegiance::TextExtent(text,[&](unsigned char ch){return font.glyphs[ch].z;});};
    float y=std::floor(origin.y+d.bounds.y);std::string_view remaining=value.empty()?std::string_view(d.text):value;auto color=authoredColor(d.color);
    auto glyph=[&](glm::vec4 g,float x,float y,float height) {
        glm::vec2 lo{x,y},hi{x+g.z,y+height};
        if(d.clipped){auto before=lo;lo=glm::max(lo,origin+glm::vec2(d.clip));hi=glm::min(hi,origin+glm::vec2(d.clip)+glm::vec2(d.clip.z,d.clip.w));g.x+=lo.x-before.x;g.y+=lo.y-before.y;}
        rect(lo.x,lo.y,hi.x-lo.x,hi.y-lo.y,color,resident.texture|(d.additive?hudAdditive:0),{g.x/font.coverage->width,g.y/font.coverage->height,(g.x+hi.x-lo.x)/font.coverage->width,(g.y+hi.y-lo.y)/font.coverage->height});
    };
    auto advance=[&](unsigned char ch){return font.glyphs[ch].z;};
    auto drawRow=[&](std::string_view row,float x,float height){
        color=authoredColor(d.color);auto baseColor=color;
        while(!row.empty()){Allegiance::InlineTextColor code;if(Allegiance::ConsumeTextColor(row,code)){color=inlineTextColor(code,color,baseColor);continue;}auto ch=static_cast<unsigned char>(row.front());row.remove_prefix(1);auto g=font.glyphs[ch];glyph(g,x,y,std::min(g.w,height));x+=g.z;}
    };
    if(d.singleLine) {
        auto row=Allegiance::ClipPaneText(remaining,int(d.bounds.z),d.justify,advance);float used=measure(row);
        float x=std::floor(origin.x+d.bounds.x+(d.justify==1?(d.bounds.z-used)*.5f:d.justify==2?d.bounds.z-used:0));
        drawRow(row,x,d.bounds.w);
        return;
    }
    while(!remaining.empty()) {
        auto line=Allegiance::ImageLine(remaining,int(d.bounds.z),advance);auto row=remaining.substr(0,line.length);
        float x=std::floor(origin.x+d.bounds.x+(d.justify==1?(d.bounds.z-measure(row))*.5f:d.justify==2?d.bounds.z-measure(row):0));
        drawRow(row,x,float(font.height));
        y+=font.height;remaining.remove_prefix(line.consumed);
    }
}
void authoredInventoryPanel(const GameState& g,HudValues values,glm::vec2 anchor) {
    using N=HudNumber;using S=HudString;
    auto draw=[&](const HudDraw& d,const ResidentHud& resident,glm::vec2 origin) {
        if(d.kind==HudDraw::Kind::Text)authoredText(d,resident.fonts[d.font],origin);
        else if(d.kind==HudDraw::Kind::Rectangle){if(d.color.a>0)rect(origin.x+d.bounds.x,origin.y+d.bounds.y,d.bounds.z,d.bounds.w,authoredColor(d.color),d.additive?hudAdditive:0);}
        else rect(origin.x+d.bounds.x,origin.y+d.bounds.y,d.bounds.z,d.bounds.w,authoredColor(d.color),authoredTextures.at(d.image.get())|(d.additive?hudAdditive:0),d.uv);
    };
    auto& shell=inventoryShell[hudStyle];
    for(const auto& surface:shell.program->evaluate(values)) {
        if(surface.kind!=HudDraw::Kind::Inventory){draw(surface,shell,anchor);continue;}
        auto origin=anchor+glm::vec2(surface.bounds);float y=origin.y;
        if(inventoryColumnTop.texture)rect(origin.x,y,inventoryColumnTop.w,inventoryColumnTop.h,glm::vec4(1),inventoryColumnTop.texture);
        y+=inventoryColumnTop.h;
        for(const auto& row:g.inventory) {
            auto& resident=authoredInventory.find(row.mdl)->second[hudStyle];
            values[N::PartRange]=row.range;values[N::PartDamage]=row.damage;values[N::PartRate]=row.rate;values[N::PartCount]=row.count;
            values[N::PartReady]=row.readiness;values[N::PartMounted]=row.mounted;values[N::PartArmed]=row.armed;
            values[N::PartSelected]=row.selected;values[N::PartActive]=row.active;values[N::PartEmpty]=row.outOfAmmo;
            values[N::PartAfterburnSpeed]=row.afterburnSpeed;values[N::PartAfterburnTime]=row.afterburnTime;values[N::PartShieldStrength]=row.shieldStrength;
            values[N::PartRechargeTime]=row.rechargeTime;values[N::PartCloakTime]=row.cloakTime;values[N::Observers]=row.observers;
            values[S::Part]=row.name;values[S::PartKey]=row.key;values[S::PartLocation]=row.location;values[S::Gunner]=row.gunner;
            for(const auto& d:resident.program->evaluate(values))draw(d,resident,{origin.x,y});
            y+=resident.program->paneSize().y;
        }
        if(inventoryColumnBottom.texture)rect(origin.x,y,inventoryColumnBottom.w,inventoryColumnBottom.h,glm::vec4(1),inventoryColumnBottom.texture);
    }
}
void authoredDraw(AuthoredRoot root,const GameState& g,const HudValues& values,glm::vec2 origin) {
    auto& resident=authored[hudStyle][root];
    for(auto& d:resident.program->evaluate(values)) {
        if(d.kind==HudDraw::Kind::Text)authoredText(d,resident.fonts[d.font],origin);
        else if(d.kind==HudDraw::Kind::Image)rect(origin.x+d.bounds.x,origin.y+d.bounds.y,d.bounds.z,d.bounds.w,authoredColor(d.color),authoredTextures.at(d.image.get())|(d.additive?hudAdditive:0),d.uv);
        else if(d.kind==HudDraw::Kind::Rectangle){if(d.color.a>0)rect(origin.x+d.bounds.x,origin.y+d.bounds.y,d.bounds.z,d.bounds.w,authoredColor(d.color),d.additive?hudAdditive:0);}
        else if(g.targetHasModel) {
            ModelPreview p;p.item=g.targetAppearance;if(d.additive)p.item.blend=ModelBlend::Additive;p.item.transform=glm::scale(glm::mat4(1),glm::vec3(1/g.targetModel.radius))*g.targetOrientation;
            auto view=glm::translate(glm::mat4(1),glm::vec3(0,0,-2.9f))*g.targetViewOrientation;
            auto projection=glm::perspectiveRH_ZO(glm::radians(46.0f),d.bounds.z/d.bounds.w,10.0f,.05f);projection[1][1]*=-1;
            p.viewProjection=projection*view;p.bounds={glm::round((origin+glm::vec2(d.bounds))*scale),glm::round(glm::vec2(d.bounds.z,d.bounds.w)*scale)};
            p.overlayOffset=quads.size();if(!preview)preview=p;else {secondPreview=p;preview->next=&*secondPreview;}
        }
    }
}
void authoredCommandPane(const GameState&,glm::vec2 anchor,bool interactive) {
    const auto& background=sprites.at("CortUI/common/commandbkgnd.png");
    float x=anchor.x,y=anchor.y-background.h;
    commandBounds=interactive?glm::vec4(x,y,background.w,background.h):glm::vec4(0);
    image("CortUI/common/commandbkgnd.png",x,y,glm::vec4(1),false);
    const auto& recipients=sprites.at("CortUI/common/recipients.png");
    for(int i=0;i<4;++i){glm::vec4 b{x+4+53*i,y+8,53,21};int face=consoleRecipient==i?1:cursor && inside(b,pointer)?2:0;
        rect(b.x,b.y,b.z,b.w,glm::vec4(1),recipients.texture,{i*.25f,face*.25f,(i+1)*.25f,(face+1)*.25f});if(interactive)hits.push_back({b,{HudCommand::CommandRecipient,i}});}
    const auto& wing=sprites.at("CortUI/common/btnchangewing3.png");glm::vec4 wingBounds{x+216,y+8,wing.w,wing.h*.5f};int wingFace=cursor && inside(wingBounds,pointer)?1:0;
    rect(wingBounds.x,wingBounds.y,wingBounds.z,wingBounds.w,glm::vec4(1),wing.texture,{0,wingFace*.5f,1,(wingFace+1)*.5f});if(interactive)hits.push_back({wingBounds,{HudCommand::CommandWing}});
    const auto& verbs=sprites.at("CortUI/common/verbs.png");
    constexpr float columns[]={0,32,62,92,122,152,182,212,242,268};
    for(int i=0;i<9;++i){glm::vec4 b{x+2+columns[i],y+38,columns[i+1]-columns[i],30};
        bool hover=cursor && inside(b,pointer);int face=!consoleVerbs[i]?3:hover?2:0;
        rect(b.x,b.y,b.z,b.w,glm::vec4(1),verbs.texture,{columns[i]/268,face*.25f,columns[i+1]/268,(face+1)*.25f});
        if(interactive && consoleVerbs[i])hits.push_back({b,{HudCommand::CommandOrder,i+1}});
        if(hover){constexpr const char* labels[]={"Attack","Capture","Defend","Pickup","Goto","Repair","Join","Mine","Build"};
            bitmapText("promptFont",x+72,y+73,labels[i],{.878f,.573f,0,1},126);}
    }
}
void authoredStatus(const GameState& g,uint32_t overlays,std::span<const int> investmentSectors={}) {
    if(!visible || modern || !g.sideActive)return;
    auto values=authoredGeometry(g,overlays);
    // GroupImage paints the first authored table entry last (on top).
    for(size_t i=overlayRecords.size();i-->0;) {
        int root=overlayRoots[hudStyle][i];if(root<0)continue;
        const auto& spec=overlayRecords[i];bool requested=(overlayFlags&spec.mask)!=0;
        auto& motion=overlayMotions[hudStyle][i];if(!motion.update(requested,overlayTime,spec.duration))continue;
        auto origin=glm::floor(motion.point(spec,overlayTime));
        if(root==RootCount)authoredInventoryPanel(g,values,origin);
        else if(root==RootCount+1)sectorMap(g,investmentSectors,overlays);
        else if(root==RootCount+2)authoredCommandPane(g,origin,requested && !spec.undetectable);
        else authoredDraw(AuthoredRoot(root),g,values,origin);
    }
}
