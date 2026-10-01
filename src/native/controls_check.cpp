#include "test_fixture.hpp"
#include "game.hpp"
#include "../ClientCore/FlightControls.h"
#include "test_reference/flight_controls.hpp"
#include "../ClientCore/KeyboardSteering.h"
#include "../ClientCore/MouseJoystick.h"
#include "../ClientCore/MouseInput.h"
#include "test_reference/mouse_keyboard.hpp"
#include "test_reference/weapon_selection.hpp"
using NativeTest::require;
namespace {
void keyboardAndMouse(){
    int mouseCases=0;
    for(unsigned overlays=0;overlays<0x20000;++overlays)for(bool enabled:{false,true})for(bool fresh:{false,true})for(bool overrideView:{false,true}){
        bool originalEnabled=enabled,originalFresh=fresh;unsigned dismiss=0;
        OriginalMouseKeyboard::toggle(originalEnabled,overlays,originalFresh,dismiss,overrideView);
        auto actual=Allegiance::ToggleMouseInput(enabled,overlays,fresh,overrideView);
        require(actual.enabled==originalEnabled && actual.freshInvestment==originalFresh && actual.dismiss==dismiss,"original mouse toggle precedence, pane mask, fresh investment and override policy");++mouseCases;
    }
    for(bool mouse:{false,true})for(bool modifier:{false,true})for(bool flip:{false,true})
    for(float yaw:{-1.f,-.3f,0.f,.7f,1.f})for(float pitch:{-1.f,0.f,.8f})for(float roll:{-.4f,0.f,.6f}){
        float joystick[]={yaw,pitch,roll},original[3];OriginalMouseKeyboard::sense(original,mouse,.25f,-.75f,joystick,modifier,flip);
        auto actual=Allegiance::JoystickSteering(mouse,.25f,-.75f,yaw,pitch,roll,modifier,flip);
        require(actual.yaw==original[0] && actual.pitch==original[1] && actual.roll==original[2],"original mouse/physical joystick source, roll modifier and flip-Y policy");++mouseCases;
    }
    printf("%d original mouse-toggle/physical-input comparisons passed\n",mouseCases);
    constexpr TrekKey keys[]={TK_RollRight,TK_RollLeft,TK_RollModifier,TK_YawRight,TK_YawLeft,TK_PitchDown,TK_PitchUp};int cases=0;
    for(int mask=0;mask<128;++mask)for(float baseline:{-.7f,0.f,.4f}){
        float a[3]={baseline,-baseline,.3f},b[3]={baseline,-baseline,.3f};std::vector<TrekKey> queriesA,queriesB;
        auto query=[&](TrekKey key){auto index=std::find(std::begin(keys),std::end(keys),key)-std::begin(keys);return (mask&(1<<index))!=0;};
        Allegiance::KeyboardSteering(a[0],a[1],a[2],[&](TrekKey key){queriesA.push_back(key);return query(key);});
        OriginalMouseKeyboard::keys(b[0],b[1],b[2],[&](TrekKey key){queriesB.push_back(key);return query(key);});
        require(std::equal(a,a+3,b) && queriesA==queriesB,"original steering signs, conflicting keys, modifier policy and query order");++cases;
    }
    for(int acceleration=0;acceleration<3;++acceleration)for(int threshold1:{0,3,6})for(int threshold2:{7,10})for(float sensitivity:{.1f,.6f,1.f,3.f})for(int delta=-128;delta<=128;++delta){
        require(Allegiance::MouseDelta(float(delta),sensitivity,acceleration,float(threshold1),float(threshold2))==OriginalMouseKeyboard::delta(delta,sensitivity,acceleration,threshold1,threshold2),"original mouse acceleration threshold and gain");++cases;
    }
    float x=0,y=0,originalX=0,originalY=0;
    for(int i=0;i<10000;++i){float dx=float(i%23-11)*3,dy=float(i%37-18)*2;Allegiance::AccumulateMouseJoystick(x,y,dx,dy,1.f/512);OriginalMouseKeyboard::move(originalX,originalY,dx,dy,1.f/512);
        require(x==originalX && y==originalY && x*x+y*y<1.000001f,"original virtual joystick cumulative motion and radial clamp");++cases;}
    Allegiance::MouseJoystick mouse;mouse.enable(true);mouse.move(200,100,1.f/512);
    require(mouse.x==0 && mouse.y==0,"enabling virtual mouse must ignore the initial recenter movement");
    mouse.move(256,-128,1.f/512);require(mouse.x==-.5f && mouse.y==.25f,"relative mouse must retain original virtual joystick direction");
    mouse.move(0,0,1.f/512);require(mouse.x==-.5f && mouse.y==.25f,"stationary virtual joystick retains turn rate");mouse.enable(false);mouse.move(100,100,1.f/512);require(mouse.x==0 && mouse.y==0,"disabled mouse clears and rejects motion");
    mouse.enable(true);mouse.move(200,200,1.f/512);require(mouse.x==0 && mouse.y==0,"resume must begin centered and suppress its first recenter event");
    printf("%d original keyboard/virtual mouse/acceleration comparisons and focus/resume checks passed\n",cases);
}
void reference(NativeTest::Fixture& f) {
    int cases=0;ControlData controls;controls.Reset();float baseline[3]={.25f,-.4f,.7f};
    for(float deadZone:{0.f,.05f,.1f,.3f,.95f})for(int axis=0;axis<3;++axis)for(int v=-1000;v<=1000;++v) {
        std::copy_n(baseline,3,controls.jsValues);controls.jsValues[axis]=v*.001f;
        require(Allegiance::AutopilotAxesChanged(controls,baseline,deadZone)==OriginalFlight::axesChanged(controls,baseline,deadZone),"original autopilot axis threshold");++cases;
    }
    for(int base:{0,int(coastButtonIGC),int(cloakActiveIGC),coastButtonIGC|cloakActiveIGC,0x1fffff})for(Mount mount=0;mount<4;++mount)for(int flags=0;flags<64;++flags) {
        int actual=Allegiance::ManualWeaponButtons(base,mount,flags&1,flags&2,flags&4,flags&8);
        actual=Allegiance::ExpendableButtons(actual,flags&16,flags&32);
        require(actual==OriginalFlight::buttons(base,mount,flags&1,flags&2,flags&4,flags&8,flags&16,flags&32),"original weapon/expendable mask");++cases;
    }
    for(auto* l=f.mission->GetHullTypes()->first();l;l=l->next())for(float fov:{1.f,10.f,25.f,50.f,65.f,90.f})for(float value:{-1.f,-.3f,0.f,.3f,1.f}) {
        auto* ship=f.ship(0,l->data()->GetObjectID());
        ControlData a;a.jsValues[0]=value;a.jsValues[1]=-value;a.jsValues[2]=.5f;a.jsValues[3]=.8f;auto b=a;
        Allegiance::LimitCockpitSlew(a,ship->GetHullType(),RadiansFromDegrees(fov),RadiansFromDegrees(50.f));
        OriginalFlight::slew(b,ship->GetHullType(),RadiansFromDegrees(fov),RadiansFromDegrees(50.f));
        require(std::equal(a.jsValues,a.jsValues+4,b.jsValues),"original FOV-dependent pitch/yaw slew");++cases;ship->Terminate();
    }
    auto* ship=f.ship();controls.Reset();controls.jsValues[0]=.6f;
    ship->SetStateM(wantsToMineMaskIGC|buildingMaskIGC|weaponsMaskIGC|coastButtonIGC);
    Allegiance::ApplyClientControls(ship,controls,rightButtonIGC|missileFireIGC);
    require(ship->GetStateM()==(wantsToMineMaskIGC|buildingMaskIGC|rightButtonIGC|missileFireIGC) && ship->GetControls().jsValues[0]==.6f,"input replaces only original client-owned state bits");
    printf("%d frozen flight control comparisons passed\n",cases);
}
void session(const std::filesystem::path& art) {
    Native::SessionOptions options;options.enemies=options.allies=0;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,10};},0,0,options);auto* ship=game.playerShip();
    game.targetKey(TK_TargetNothing);
    game.clearMessages();game.clearSounds();
    Native::FlightInput input;input.yaw=.5f;game.toggleAutopilot();game.step(input,.001);
    require(ship->GetAutopilot(),"engaging autopilot captures a deflected stick");
    require(game.state().messages.empty() && std::count_if(game.sounds().begin(),game.sounds().end(),[](const auto& sound){return sound.id==salAutopilotEngageSound;})==1,"autopilot engagement loses original spoken feedback or adds substitute console text");
    input.yaw=.6f;game.step(input,.001);require(ship->GetAutopilot(),"small stick jitter retains autopilot");
    input.yaw=.5f;input.pitch=.21f;input.active=false;game.step(input,.001);require(ship->GetAutopilot(),"inactive UI input cannot disengage autopilot");
    input.active=true;game.step(input,.001);require(!ship->GetAutopilot(),"axis delta above twice dead zone disengages");
    require(game.state().messages.empty() && std::count_if(game.sounds().begin(),game.sounds().end(),[](const auto& sound){return sound.id==salAutopilotDisengageSound;})==1,"manual autopilot cancellation loses original spoken feedback or adds substitute console text");
    float fallback;require(game.takeThrottleReset(fallback) && (fallback==0 || fallback==1) && !game.takeThrottleReset(fallback),"autopilot release queues one throttle handoff");
    for(int control=0;control<11;++control) {
        input={};game.step(input,.001);game.toggleAutopilot();game.step(input,.001);
        switch(control) {
        case 0:input.forward=true;break;case 1:input.backward=true;break;case 2:input.left=true;break;
        case 3:input.right=true;break;case 4:input.up=true;break;case 5:input.down=true;break;
        case 6:input.boost=true;break;case 7:input.fire=true;break;case 8:input.missile=true;break;
        case 9:input.matchSpeed=true;break;case 10:input.throttleChanged=true;break;
        }
        game.step(input,.001);require(!ship->GetAutopilot(),"original manual action disengages autopilot");
    }
    input={};input.fire=true;game.step(input,.001);game.toggleAutopilot();input.fire=false;game.step(input,.001);
    require(!ship->GetAutopilot(),"fire-button release edge also disengages original autopilot");
    input={};input.fire=true;game.step(input,.001);game.respawn();
    require(game.returnToBase(),"return-to-base after respawn accepts a base command");game.step({},.001);
    require(ship->GetAutopilot(),"held fire from the previous life cannot cancel return-to-base");game.toggleAutopilot();
    input={};game.step(input,.001);game.toggleVectorLock();
    require(game.state().coast,"vector lock toggles on");
    for(int i=0;i<10;++i)game.step(input,.01);
    require(game.state().coast,"vector lock persists after key release");game.toggleVectorLock();require(!game.state().coast,"vector lock toggles off");
    game.toggleCloak();require(ship->GetStateM()&cloakActiveIGC,"cloak is an IGC state toggle, including hulls without a cloak");
    game.step(input,.001);require(ship->GetStateM()&cloakActiveIGC,"client preserves cloak state");game.toggleCloak();require(!(ship->GetStateM()&cloakActiveIGC),"cloak toggles off");
    game.toggleAutopilot();game.step(input,.001);
    auto amount=[&](Native::PartKind kind){for(auto& e:game.state().equipment)if(e.kind==kind)return e.amount;return -1;};
    const int chaff=amount(Native::PartKind::Chaff),mines=amount(Native::PartKind::Dispenser);
    input.chaff=input.deploy=true;
    for(int i=0;i<80;++i)game.step(input,.01);
    require(ship->GetAutopilot() && chaff>0 && mines>0 && amount(Native::PartKind::Chaff)<chaff && amount(Native::PartKind::Dispenser)<mines,"mine/chaff deploy through real IGC while autopilot stays engaged");
    input={};game.step(input,.001);require(!(ship->GetStateM()&(mineFireIGC|chaffFireIGC)),"expendable release clears stale autopilot fire bits");
    game.toggleAutopilot();input={};input.yaw=.35f;input.pitch=.25f;game.step(input,.01);
    require(game.state().steering.x==ship->GetControls().jsValues[c_axisYaw] && game.state().steering.y==ship->GetControls().jsValues[c_axisPitch],"steering indicator snapshot must reflect actual approved/limited IGC controls");
    puts("Native autopilot baseline/jitter/action cancellation, UI focus, persistent toggles and expendables passed");
}
void networkWeaponCycle(const std::filesystem::path& art) {
    using namespace Native;
    SessionOptions options;options.scenario=Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
    auto loader=[](auto&,auto&){return ModelHandle{1,1};};
    Game authority(art,loader,0,0,options,{},GameRole::Authority),replica(art,loader,0,0,options,{},GameRole::Replica);
    auto* ship=authority.playerShip();auto* home=ship->GetStation();
    ship->SetStation(nullptr);ship->SetCluster(home->GetCluster());ship->SetPosition(Vector(50000,0,0));
    int pilot=ship->GetObjectID(),cases=0;Net::Tick tick=60;
    std::vector<Net::Action> actions;replica.networkActions([&](const auto& action){actions.push_back(action);return true;});actions.clear();
    auto report=[&]{auto frame=authority.networkFrame(pilot,++tick,0);return Net::frame(Net::packet(frame));};
    auto checkReport=[&](const Net::Frame& frame,int selected,bool group){
        require(frame.status.selectedWeapon==selected && bool(frame.status.groupFire)==group,"authority loses selected hardpoint or changes group mode");
        require(std::none_of(frame.events.begin(),frame.events.end(),[](const auto& event){return event.kind==Net::EventKind::Sound && OriginalWeaponTest::cue(event.value);}),"local weapon-selection cues are broadcast by authority");
    };
    for(unsigned mask:{1u,3u,5u,7u})for(bool group:{false,true}){
        OriginalWeaponTest::fit(authority,ship,mask);authority.authorityStep({},0);
        // Normalize the real actor via the same commands as the client.
        auto status=report().status;if(!status.groupFire)authority.selectWeapon(-1);
        authority.selectWeapon(0);if(group)authority.selectWeapon(-1);
        auto baseline=report();checkReport(baseline,0,group);replica.applyNetworkFrame(baseline);
        require(!authority.state().docked && !replica.state().docked,"weapon-cycle fixture must be in flight");
        int selected=0;replica.clearSounds();actions.clear();
        for(bool previous:{false,false,true,true,false,true}){
            auto stale=report();int expected=OriginalWeaponTest::next(mask,selected,previous);
            replica.cycleWeapon(previous);OriginalWeaponTest::hud(replica,expected,group);
            require(actions.size()==1 && actions.front().op==Net::Operation::Weapon && actions.front().args[0]==expected && actions.front().args[1]==int(group),"next/previous sends its chosen hardpoint and preserved group mode");
            auto action=actions.front();actions.clear();
            // A lagging report restores canonical state; its unincorporated
            // local action must be reapplied without another command or cue.
            replica.applyNetworkFrame(stale);replica.replayWeaponAction(action);replica.replayWeaponAction(Net::Action{.op=Net::Operation::Target});
            OriginalWeaponTest::hud(replica,expected,group);
            require(actions.empty() && !OriginalWeaponTest::cues(replica),"reconciliation resends weapon commands or invents cycling cues");
            require(authority.authorityAction(pilot,action),"weapon cycle reaches authority");selected=expected;
            auto canonical=report();checkReport(canonical,selected,group);replica.applyNetworkFrame(canonical);OriginalWeaponTest::hud(replica,selected,group);
            FlightInput input;input.fire=true;
            std::array<std::pair<int,FlightInput>,1> inputs{{{pilot,input}}};authority.authorityStep(inputs,0);replica.predict(input,0,false);
            for(auto* actor:{ship,replica.playerShip()})
                require((actor->GetStateM()&weaponsMaskIGC)==(group?allWeaponsIGC:oneWeaponIGC) && ((actor->GetStateM()&selectedWeaponMaskIGC)>>selectedWeaponShiftIGC)==selected,
                        "actual local/server firing bits disagree with HUD after weapon cycling");
            ++cases;
        }
        // A new replica must restore the latent grouped hardpoint as well as
        // the visible single mode; ungrouping must select the remembered gun.
        Game fresh(art,loader,0,0,options,{},GameRole::Replica);fresh.applyNetworkFrame(report());OriginalWeaponTest::hud(fresh,selected,group);
        fresh.selectWeapon(-1);OriginalWeaponTest::hud(fresh,selected,!group);
        require(OriginalWeaponTest::cues(fresh)==1 && fresh.sounds().back().id==(!group?groupWeaponsSound:ungroupWeaponsSound),"original local toggle cue or hidden selection is lost on bootstrap");
    }
    // Client input frames can run ahead of the server. Selecting the same gun
    // groups locally even though the authority's last physical input still
    // contains the old hardpoint; replay must preserve the actual key result.
    OriginalWeaponTest::fit(authority,ship,5);authority.selectWeapon(0);authority.authorityStep({},0);
    auto single=report();checkReport(single,0,false);replica.applyNetworkFrame(single);replica.clearSounds();actions.clear();
    replica.cycleWeapon();require(actions.size()==1 && authority.authorityAction(pilot,actions.front()),"lagged cycle reaches authority");actions.clear();
    replica.predict({},0,false);
    require(((ship->GetStateM()&selectedWeaponMaskIGC)>>selectedWeaponShiftIGC)==0 && ((replica.playerShip()->GetStateM()&selectedWeaponMaskIGC)>>selectedWeaponShiftIGC)==2,
            "latency regression must retain different local/server physical input frames");
    replica.selectWeapon(2);OriginalWeaponTest::hud(replica,2,true);require(actions.size()==1,"same-gun grouping intent captured");auto grouped=actions.front();actions.clear();
    auto stale=report();replica.applyNetworkFrame(stale);replica.replayWeaponAction(grouped);OriginalWeaponTest::hud(replica,2,true);
    require(authority.authorityAction(pilot,grouped),"same-gun selection reaches lagged authority");checkReport(report(),2,true);
    require(actions.empty() && OriginalWeaponTest::cues(replica)==1 && replica.sounds().back().id==groupWeaponsSound,"lagged selection must retain one original local cue");
    int docked=authority.addLocalPilot("Docked actor",0);require(docked>=0 && authority.controlLocalPilot(docked),"docked authority context fixture");
    require(authority.playerShip()->GetStation() && authority.state().docked && !ship->GetStation(),"authority context must differ from authenticated caller's station state");
    Net::Action desired;desired.op=Net::Operation::Weapon;desired.args={2,1};
    require(authority.authorityAction(pilot,desired),"flying caller inherits another pilot's cached docked guard");
    require(!authority.authorityAction(docked,desired),"docked caller can submit a flight weapon selection");
    Net::Action autopilot;autopilot.op=Net::Operation::Autopilot;ship->SetAutopilot(false);
    require(authority.authorityAction(pilot,autopilot) && ship->GetAutopilot(),"flying caller's autopilot inherits a different docked actor's cached state");
    require(authority.authorityAction(pilot,autopilot) && !ship->GetAutopilot(),"flying caller cannot disengage autopilot in a docked authority context");
    require(authority.authorityAction(docked,autopilot) && !authority.core()->GetShip(docked)->GetAutopilot(),"docked caller's autopilot must remain off");
    auto baseline=report();
    for(auto bad:std::array<std::array<int32_t,4>,5>{{{-1,0},{4,0},{INT_MAX,0},{0,-1},{0,2}}}){Net::Action action;action.op=Net::Operation::Weapon;action.args=bad;require(!authority.authorityAction(pilot,action),"invalid desired weapon selection accepted");checkReport(report(),baseline.status.selectedWeapon,baseline.status.groupFire);}
    for(int invalid=0;invalid<3;++invalid){auto bad=baseline;if(invalid==0)bad.status.groupFire=2;else bad.status.selectedWeapon=invalid==1?-1:c_maxMountedWeapons;bool rejected=false;try{Net::frame(Net::packet(bad));}catch(const std::exception&){rejected=true;}require(rejected,"malformed canonical weapon selection accepted");}
    printf("Original weapon selection: %d sparse/single/group next/previous cases, real firing bits, inventory highlights, stale-report replay, lagged same-gun grouping, authenticated actor guards, silent authority, eight bootstraps and malformed intents/records passed\n",cases);
}
}
int main(int argc,char** argv) {
    try{keyboardAndMouse();auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();{NativeTest::Fixture f(art);reference(f);}session(art);networkWeaponCycle(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"controls: %s\n",e.what());return 1;}
}
