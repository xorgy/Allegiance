#include "test_fixture.hpp"
#include "game.hpp"
#include "smoke.hpp"
#include "command_input.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../ClientCore/PlayerCommands.h"
#include "test_reference/player_commands.hpp"
#include "../ClientCore/QuickChat.h"
#include "../ClientCore/ChatRouting.h"
#include "test_reference/quick_chat.hpp"
#include "../ClientCore/QueuedRequests.h"
#include "test_reference/queued_requests.hpp"
#include <climits>
using NativeTest::require;
namespace {
void reference(NativeTest::Fixture& f) {
    auto* station=f.mission->GetSide(0)->GetStations()->first()->data();auto* friendly=f.ship();auto* enemy=f.ship(1);friendly->SetWingID(3);
    int cases=0,accepted=0;
    for(CommandID command:{c_cidNone,c_cidGoto,c_cidDefend,c_cidAttack,c_cidJoin,c_cidPickup})for(bool execute:{false,true})for(bool docked:{false,true})for(bool gunner:{false,true})for(bool reachable:{false,true}) {
        auto* a=f.ship();auto* b=f.ship();auto* target=command==c_cidAttack?enemy:friendly;
        for(auto* ship:{a,b}) {
            if(docked){ship->SetCluster(nullptr);ship->SetStation(station);}
            if(gunner)ship->SetParentShip(friendly);
            ship->SetCommand(c_cmdQueued,target,command);
        }
        auto run=[&](IshipIGC* ship,bool original) {
            std::vector<int> trace;
            auto sound=[&]{trace.push_back(1);};auto join=[&](IshipIGC* target){trace.push_back(2);ship->SetWingID(target->GetWingID());};auto rescue=[&](IshipIGC*){trace.push_back(3);};
            auto select=[&](ImodelIGC* target,CommandID command){trace.push_back(4);ship->SetCommand(c_cmdCurrent,target,command);};
            auto accept=[&](ImodelIGC* target,CommandID command){trace.push_back(5);ship->SetCommand(c_cmdAccepted,target,command);};
            auto canReach=[&](ImodelIGC*){trace.push_back(6);return reachable;};auto engage=[&]{trace.push_back(7);ship->SetAutopilot(true);};
            if(original)OriginalCommands::queued(ship,execute,sound,join,rescue,select,accept,canReach,engage);
            else accepted+=Allegiance::AcceptQueuedOrder(ship,execute,sound,join,rescue,select,accept,canReach,engage);
            return trace;
        };
        require(run(a,true)==run(b,false),"original queued command callbacks/order differ");
        for(Command slot=0;slot<c_cmdMax;++slot)require(a->GetCommandID(slot)==b->GetCommandID(slot) && a->GetCommandTarget(slot)==b->GetCommandTarget(slot),"original current/accepted/queued/plan transitions differ");
        require(a->GetWingID()==b->GetWingID() && a->GetAutopilot()==b->GetAutopilot(),"original wing/autopilot transitions differ");
        if(gunner){a->SetParentShip(nullptr);b->SetParentShip(nullptr);}++cases;
    }
    require(accepted>0,"reference matrix exercised no legal orders");
    printf("%d frozen queued-command cases, %d accepted, passed\n",cases,accepted);
    int rejected=0;
    for(Money request:{-1,0,1})for(CommandID command:{c_cidDoNothing,c_cidNone,c_cidDefend})for(bool clear:{false,true})for(bool negative:{false,true})for(auto* sender:{static_cast<IshipIGC*>(nullptr),friendly,enemy}) {
        auto run=[&](bool original){std::vector<int> trace;auto clearMoney=[&]{trace.push_back(1);};auto clearOrder=[&]{trace.push_back(2);};
            auto getSender=[&]{trace.push_back(3);return sender;};auto clearSender=[&]{trace.push_back(4);};auto reply=[&](IshipIGC* from){require(from==sender,"reject sender identity");trace.push_back(5);};
            bool handled=original?OriginalQueuedRequests::reject(request,command,clear,negative,friendly,clearMoney,clearOrder,getSender,clearSender,reply):Allegiance::RejectQueuedRequest(request,command,clear,negative,friendly,clearMoney,clearOrder,getSender,clearSender,reply);
            trace.push_back(handled?6:7);return trace;
        };
        require(run(true)==run(false),"frozen original money/order rejection, timeout and callback ordering");++rejected;
    }
    for(Money requested:{INT_MIN,-1,0,1,SHRT_MAX,INT_MAX})for(Money available:{INT_MIN,-1,0,1,SHRT_MAX,INT_MAX})
        require(Allegiance::ApprovedMoneyRequest(requested,available)==OriginalQueuedRequests::approve(requested,available),"original partial/zero money request approval");
    for(int flags=0;flags<8;++flags){bool pickup=flags&1,same=flags&2,rescue=flags&4;int calls=0;
        require(Allegiance::RecoveryOrderTip(pickup,same,[&]{++calls;return rescue;})==(pickup && same && rescue) && calls==int(pickup && same),"original short-circuit recovery tip avoids probing unrelated/gunner hulls");
    }
    printf("%d frozen rejection/timeout callback sequences and 36 money approval limits passed\n",rejected);
}
void chatRouting(NativeTest::Fixture& f) {
    auto* own=f.mission->GetSide(0);auto* enemy=f.mission->GetSide(1);
    int cases=0;
    for(int channel=-1;channel<=CHAT_MAX;++channel)for(auto* sender:{static_cast<IsideIGC*>(nullptr),own,enemy}) {
        bool expected;
        switch(channel){
        case CHAT_SHIP:case CHAT_WING:case CHAT_FRIENDLY_SECTOR:expected=true;break;
        case CHAT_TEAM:expected=!sender || sender==own;break;
        case CHAT_INDIVIDUAL:case CHAT_INDIVIDUAL_NOFILTER:case CHAT_GROUP:case CHAT_GROUP_NOECHO:expected=!sender || sender==own || IsideIGC::AlliedSides(sender,own);break;
        default:expected=false;
        }
        require(Allegiance::ChatOrderAuthorized(ChatTarget(channel),sender,own)==expected,"frozen server channel command/side authority");++cases;
    }
    own->SetAllies(0);enemy->SetAllies(0);
    require(Allegiance::ChatOrderAuthorized(CHAT_INDIVIDUAL,enemy,own) && !Allegiance::ChatOrderAuthorized(CHAT_TEAM,enemy,own),"allied individual orders do not grant cross-team broadcast authority");
    own->SetAllies(NA);enemy->SetAllies(NA);
    printf("%d frozen chat-order route/authority cases passed\n",cases);
}
void picking() {
    unsigned cases=0;
    for(float bz:{-1.f,-.91f,-.9f,-.5f,0.f,.5f,.9f,.91f,1.f})
    for(float cx:{-4200.f,0.f,6000.f})for(float cy:{-500.f,0.f,9000.f})
    for(float rx:{-1.f,-.01f,.1f,1.f})for(float ry:{-1.f,0.f,.1f,1.f})for(float rz:{-1.f,0.f,1.f}) {
        float expected=bz>.9f || bz<-.9f?0:((150-cx)*rx*rz+(200-cy)*ry*rz)/(rx*rx+ry*ry)+8000;
        float actual=Allegiance::CommandPickAltitude(150,200,cx,cy,8000,bz,rx,ry,rz);
        require(expected==actual,"original command height equation differs");++cases;
    }
    require(Allegiance::CommandPickAltitude(0,0,0,0,0,0,0,0,1)==0,"vertical command ray remains finite");
    for(int dx=-8;dx<=8;++dx)for(int dy=-8;dy<=8;++dy)
        require(Allegiance::CommandSelectionRectangle(float(dx),float(dy))==(abs(dx)>5 || abs(dy)>5),"original drag threshold");
    Native::CameraState camera;camera.position={0,-6000,6000};camera.forward=glm::normalize(-camera.position);camera.up={0,0,1};
    glm::ivec2 size{2560,1440};auto projection=glm::perspectiveRH_ZO(glm::radians(65.f),float(size.x)/size.y,30000.f,.2f);projection[1][1]*=-1;
    auto vp=projection*glm::lookAtRH(camera.position,camera.position+camera.forward,camera.up);
    Native::CommandGesture gesture;gesture.beginOrder(glm::vec2(size)*.5f,size,vp,camera);
    require(gesture.anchor && glm::length(*gesture.anchor)<.01f,"command plane ray intersects origin");
    gesture.pointer.y-=180;auto height=gesture.destination(size,vp,camera);require(height && height->z>0,"upward command drag selects positive height");
    auto clip=vp*glm::vec4(*height,1);auto pixel=(glm::vec2(clip)/clip.w+1.f)*.5f*glm::vec2(size);
    require(glm::distance(pixel,gesture.pointer)<.01f,"command height projects back to the cursor");
    std::array<Native::Contact,4> contacts{};
    for(int i=0;i<4;++i){contacts[i].id=i;contacts[i].type=Native::ContactShip;contacts[i].radius=20;contacts[i].position={0,0,0};contacts[i].own=true;}
    contacts[1].own=false;contacts[1].enemy=true;contacts[2].position=camera.position-camera.forward*500.f;contacts[3].own=false;contacts[3].ally=true;
    require(Native::commandRectangle(contacts,{1200,650},{1360,800},size,vp)==std::vector<int>({0,3}),"rectangle selects visible friendly/allied ships, excludes enemy and behind-camera contacts");
    printf("%u original command altitude comparisons and native selection projection passed\n",cases);
}
void session(const std::filesystem::path& art) {
    Native::Assets assets(art);Native::SessionOptions options;options.enemies=0;options.startInHangar=true;options.scenario=Native::Scenario::FreeFlight;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* core=game.core();auto* player=game.playerShip();auto* station=player->GetStation();
    int friendId=game.addLocalPilot("Commander",0);require(friendId>=0,"local command sender");auto* sender=core->GetShip(friendId);sender->SetWingID(4);
    game.launch();sender->SetStation(nullptr);sender->SetCluster(player->GetCluster());sender->SetPosition(player->GetPosition()+Vector(300,0,0));
    auto queue=[&](CommandID command,ImodelIGC* target) {core->GetIgcSite()->ReceiveChat(sender,CHAT_INDIVIDUAL,player->GetObjectID(),NA,"Order",command,target->GetObjectType(),target->GetObjectID(),target);game.step({},.001);};
    queue(c_cidGoto,station);require(game.state().commands[0].command==c_cidGoto,"incoming command snapshot");
    require(game.acceptCommand() && player->GetCommandTarget(c_cmdAccepted)==station && player->GetCommandTarget(c_cmdCurrent)==station && !player->GetAutopilot(),"accept stores current and accepted without executing");
    game.selectTarget(OT_ship,friendId);
    require(player->GetCommandTarget(c_cmdAccepted)==station && player->GetCommandTarget(c_cmdCurrent)==sender && player->GetCommandID(c_cmdCurrent)==player->GetDefaultOrder(sender),"target selection preserves accepted command and uses original default order");
    game.targetCommand();require(player->GetCommandTarget(c_cmdCurrent)==station,"target accepted command");
    queue(c_cidGoto,sender);game.rejectCommand();require(player->GetCommandID(c_cmdQueued)==c_cidNone && player->GetCommandTarget(c_cmdAccepted)==station,"reject affects queued slot only");
    queue(c_cidGoto,station);require(game.acceptCommand(true) && player->GetAutopilot() && player->GetCommandTarget(c_cmdPlan)==station,"execute starts original current-target navigation");
    game.toggleAutopilot();game.clearCommand();require(player->GetCommandID(c_cmdAccepted)==c_cidNone && player->GetCommandTarget(c_cmdCurrent)==station,"clear accepted preserves current target");
    queue(c_cidJoin,sender);require(game.acceptCommand(true) && player->GetWingID()==4 && !player->GetAutopilot() && player->GetCommandTarget(c_cmdCurrent)==station,"join adopts wing without navigation target change");
    game.selectTarget(OT_ship,friendId);sender->SetVelocity(Vector(10,0,0));game.step({},.001);
    float expected=sender->GetVelocity().Length()/player->GetHullType()->GetMaxSpeed();require(std::abs(game.matchTargetThrottle(0)-std::min(expected,1.f))<1e-5,"original match-speed throttle");
    queue(c_cidGoto,station);for(int i=0;i<151;++i)game.step({},.1);require(player->GetCommandID(c_cmdQueued)==c_cidGoto && player->GetCommandTarget(c_cmdQueued)==station,"original timeout retains queued command/target");
    game.clearSounds();require(game.acceptCommand() && std::none_of(game.sounds().begin(),game.sounds().end(),[](auto& event){return event.id==voAffirmativeSound;}),"late order acceptance does not acknowledge an expired sender");
    player->SetAutopilot(false);
    require(game.order(player->GetObjectID(),OT_station,station->GetObjectID(),c_cidGoto) && player->GetAutopilot() && player->GetCommandTarget(c_cmdPlan)==station,"individual self-order accepts, targets and executes");
    sender->SetAutopilot(false);sender->SetCommand(c_cmdAccepted,nullptr,c_cidNone);
    require(game.order(friendId,OT_station,station->GetObjectID(),c_cidGoto) && sender->GetCommandID(c_cmdQueued)==c_cidGoto && sender->GetCommandID(c_cmdAccepted)==c_cidNone && !sender->GetAutopilot(),"order to another pilot awaits acceptance");
    require(game.controlLocalPilot(friendId) && game.state().commands[0].command==c_cidGoto && game.acceptCommand(true) && sender->GetCommandTarget(c_cmdPlan)==station && sender->GetAutopilot(),"pending order follows pilot control and original execution");
    require(game.controlLocalPilot(player->GetObjectID()),"restore original pilot");
    require(game.order(friendId,OT_station,station->GetObjectID(),c_cidGoto),"requeue inactive pilot order");
    for(int i=0;i<151;++i)game.step({},.1);
    require(sender->GetCommandID(c_cmdQueued)==c_cidGoto,"inactive local pilot timeout retains its queued command too");
    Native::QuickChatCatalog catalog(assets);game.quickChats(catalog);int quickCases=0;
    for(auto& entry:catalog.commands)for(int selection=0;selection<3;++selection) {
        player->SetCommand(c_cmdCurrent,selection==0?nullptr:selection==1?static_cast<ImodelIGC*>(sender):station,c_cidGoto);
        player->SetCommand(c_cmdAccepted,station,c_cidGoto);player->SetCommand(c_cmdQueued,sender,c_cidDefend);
        CommandID expected=CommandID(entry.command),actual=expected;
        auto* a=OriginalQuickChat::target(player,expected,entry.targetMask,AbilityBitMask(entry.abilities));
        auto* b=Allegiance::QuickChatTarget(player,actual,entry.targetMask,AbilityBitMask(entry.abilities));
        require(a==b && expected==actual,"frozen original quick-chat target selection");++quickCases;
    }
    auto affirmative=std::find_if(catalog.commands.begin(),catalog.commands.end(),[](auto& entry){return entry.text.find("Affirmative")!=std::string::npos;});
    require(affirmative!=catalog.commands.end(),"original affirmative quick chat missing");
    game.clearMessages();game.clearSounds();
    require(game.sendQuickChat(Native::ChatChannel::Target,*affirmative,friendId),"quick chat to a valid pilot");
    require(!game.state().messages.empty() && !game.sounds().empty(),"outgoing individual quick chat must echo text and voice locally");
    require(!game.sendQuickChat(Native::ChatChannel::Target,*affirmative,65536+friendId),"quick chat rejects a truncated recipient ID");
    {
        int otherWing=game.addLocalPilot("Other wing",0);require(otherWing>=0,"additional broadcast recipient");auto* outside=core->GetShip(ShipID(otherWing));
        sender->SetStation(nullptr);sender->SetCluster(player->GetCluster());outside->SetStation(nullptr);outside->SetCluster(player->GetCluster());
        player->SetWingID(4);sender->SetWingID(4);outside->SetWingID(5);
        Native::QuickChatCommand defend;defend.command=c_cidDefend;defend.targetMask=c_ttMe;defend.text="Defend me";
        auto clear=[&]{for(auto* ship:{player,sender,outside})ship->SetCommand(c_cmdQueued,nullptr,c_cidNone);};
        clear();require(game.sendQuickChat(Native::ChatChannel::Team,defend) && sender->GetCommandID(c_cmdQueued)==c_cidDefend && outside->GetCommandID(c_cmdQueued)==c_cidDefend,"team quick command reaches inactive local pilots");
        require(game.controlLocalPilot(friendId) && game.state().commands[0].command==c_cidDefend && game.acceptCommand() && sender->GetCommandTarget(c_cmdAccepted)==player,"broadcast order follows selected pilot and awaits real acceptance");
        require(game.controlLocalPilot(player->GetObjectID()),"restore broadcast sender");
        clear();require(game.sendQuickChat(Native::ChatChannel::Wing,defend) && sender->GetCommandID(c_cmdQueued)==c_cidDefend && outside->GetCommandID(c_cmdQueued)==c_cidNone,"wing commands exclude other wings");
        clear();require(game.sendQuickChat(Native::ChatChannel::FriendlySector,defend) && sender->GetCommandID(c_cmdQueued)==c_cidDefend && outside->GetCommandID(c_cmdQueued)==c_cidDefend,"friendly-sector orders reach flying pilots");
        outside->SetCluster(nullptr);outside->SetStation(station);clear();game.sendQuickChat(Native::ChatChannel::FriendlySector,defend);
        require(sender->GetCommandID(c_cmdQueued)==c_cidDefend && outside->GetCommandID(c_cmdQueued)==c_cidNone,"friendly-sector orders exclude docked pilots");
        for(auto channel:{Native::ChatChannel::Everyone,Native::ChatChannel::Allies,Native::ChatChannel::Sector,Native::ChatChannel::Leaders}){clear();require(game.sendQuickChat(channel,defend) && sender->GetCommandID(c_cmdQueued)==c_cidNone && outside->GetCommandID(c_cmdQueued)==c_cidNone,"public broadcast command must not bypass original server restrictions");}
        clear();core->GetIgcSite()->ReceiveChat(sender,CHAT_EVERYONE,NA,NA,"Invalid public order",c_cidDefend,OT_ship,friendId,sender);
        require(player->GetCommandID(c_cmdQueued)==c_cidNone,"incoming public command rejected before local order queue");
        clear();core->GetIgcSite()->SendChat(player,CHAT_INDIVIDUAL,friendId,NA,"Default order",c_cidDefault,OT_station,station->GetObjectID(),station,false);
        require(sender->GetCommandID(c_cmdQueued)==sender->GetDefaultOrder(station),"inactive individual recipient resolves their own default order");
        clear();outside->Terminate();sender->SetCommand(c_cmdAccepted,nullptr,c_cidNone);
    }
    printf("%zu authored quick-chat menus, %zu commands, %d frozen target comparisons passed\n",catalog.menus.size(),catalog.commands.size(),quickCases);
    game.commandView(true);require(game.pickCommandUnit(player->GetObjectID()),"select local pilot for group");
    require(game.pickCommandUnit(friendId,true) && game.commandUnits().size()==2 && game.commandUnits().front()==player->GetObjectID(),"shift selection preserves self first");
    require(game.pickCommandUnit(player->GetObjectID(),true) && game.commandUnits().size()==1,"shift deselect one member");
    require(game.pickCommandUnit(friendId,true) && game.commandUnits().size()==1,"last group member cannot be toggled away");
    int ids[]={friendId,player->GetObjectID(),friendId,-1,65536+friendId};game.pickCommandUnits(ids);
    require(game.commandUnits().size()==2 && game.state().commandUnitCount==2 && game.commandUnits().front()==player->GetObjectID(),"rectangle group deduplicates and validates IDs");
    game.commandOrder(c_cidGoto);require(game.commandPosition({1000,500,350}),"group accepts one shared waypoint");
    auto* waypoint=player->GetCommandTarget(c_cmdAccepted);
    require(waypoint && waypoint->GetObjectType()==OT_buoy && sender->GetCommandTarget(c_cmdQueued)==waypoint,"group shares waypoint, other pilot still must accept");
    game.pickCommandUnits(std::span(&friendId,1));sender->Terminate();game.step({},.001);
    require(game.commandUnits().empty() && game.state().commandUnitCount==0,"terminated recipient removed from selection");
    puts("Native incoming orders, acceptance, target separation, execute, reject, join, match speed and expiry passed");
}
}
namespace {
void battleObserver(const std::filesystem::path& art) {
    Native::SessionOptions options;options.scenario=Native::Scenario::Skirmish;
    options.allies=options.enemies=8;options.observerBattleDepth=1400;options.killLimit=0;options.variedWingmen=true;
    Native::Assets assets(art);
    Native::Game game(art,[&](const std::string& name,const std::string&){auto model=assets.model(name);return Native::ModelHandle{1,model->radius};},0,0,options);
    Native::arrangeScaleBattle(game,false,true,6000,true);
    auto* core=game.core();auto* observer=game.playerShip();
    require(game.state().commandView && game.state().docked && !observer->GetCluster(),"command observer is a docked pilot, not a combatant");
    require(core->GetShips()->n()==17 && observer->GetStation()->GetCluster()->GetShips()->n()==16,"both AI teams exist without the observer in space");
    for(int side=0;side<2;++side) {
        std::array<int,Native::battleHullIds.size()> counts{};
        std::vector<IshipIGC*> ships;
        for(auto* l=core->GetSide(side)->GetShips()->first();l;l=l->next())if(l->data()!=observer) {
            auto* bot=l->data();require(bot->GetMountedPart(ET_Weapon,0),"mixed fleet hull has its original preferred gun");
            auto hull=std::find(Native::battleHullIds.begin(),Native::battleHullIds.end(),bot->GetBaseHullType()->GetObjectID());
            require(hull!=Native::battleHullIds.end(),"mixed fleet uses supported combat hulls");++counts[hull-Native::battleHullIds.begin()];
            for(auto* previous:ships)require((bot->GetPosition()-previous->GetPosition()).Length()>=std::max(72.f,bot->GetRadius()+previous->GetRadius()),"tightened initial fleet spacing avoids overlapping ships");
            ships.push_back(bot);
        }
        for(auto count:counts)require(count==2,"both sides have equal proportions of all four hulls");
    }
    auto* ally=core->GetSide(0)->GetShips()->last()->data();auto* enemy=core->GetSide(1)->GetShips()->last()->data();
    observer->SetCommand(c_cmdCurrent,enemy,c_cidAttack);
    for(auto* victim:{ally,enemy})core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),victim,nullptr,1,Vector::GetZero(),Vector::GetZero());
    game.step({},.001);require(core->GetShips()->n()==15,"observer test removes two real AI ships");
    for(int i=0;i<1400 && core->GetShips()->n()<17;++i)game.step({},1./165);
    require(core->GetShips()->n()==17,"AI replacements continue while the observer is docked");
    require(observer->GetStation() && !observer->GetCluster() && game.state().commandView,"AI replacements do not launch or kill the observer");
    bool front=false,back=false;
    for(auto* l=core->GetShips()->first();l;l=l->next())if(l->data()!=observer) {
        auto* bot=l->data();require(bot->GetCluster()==observer->GetStation()->GetCluster(),"replacements spawn in the viewed battle sector");
        require(bot->GetMountedPart(ET_Weapon,0),"replacement mixed hull retains its original preferred gun");
        for(Command c=0;c<c_cmdMax;++c)require(bot->GetCommandTarget(c)!=observer,"AI orders do not target or defend the docked observer");
        if(bot->GetSide()->GetObjectID()==0 && bot->GetPosition().z==-1400)back=true;
        if(bot->GetSide()->GetObjectID()==1 && bot->GetPosition().z==1400)front=true;
    }
    require(front && back,"replacement spawn planes stay on opposing sides");
    Native::CameraInput input;const auto& camera=game.camera(input);
    require(camera.mode==Native::CameraMode::Command && std::abs(glm::length(camera.position)-6000)<.1f,"observer camera frames the encounter center");
    puts("Docked battle observer, balanced armed mixed fleets, tighter spacing, independent replacement planes and command camera passed");
}
}
int main(int argc,char** argv) {
    try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();{NativeTest::Fixture f(art);reference(f);chatRouting(f);}picking();session(art);battleObserver(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"commands: %s\n",e.what());return 1;}
}
