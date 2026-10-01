#include "game.hpp"
#include "legacy.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/CameraMotion.h"
#include "../ClientCore/CameraFraming.h"
#include "../ClientCore/CameraZoom.h"
#include "../ClientCore/ShipSound.h"
#include <stdexcept>
#include <cstdio>

using namespace Allegiance;
namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void near(float actual, float expected, const char *message, float tolerance = 2e-5f)
{
    if (std::abs(actual - expected) > tolerance || !std::isfinite(actual))
    {
        fprintf(stderr, "%s: got %.9g, expected %.9g\n", message, actual, expected);
        throw std::runtime_error(message);
    }
}
void near(const Vector &a, const Vector &b, const char *message)
{
    near((a - b).Length(), 0, message);
}
void near(glm::vec3 a, glm::vec3 b, const char *message)
{
    near(glm::length(a - b), 0, message, .002f);
}
void soundPolicy(const std::filesystem::path& art) {
    NativeTest::Fixture fixture(art);auto* ship=fixture.ship();auto* hull=ship->GetHullType();
    ship->SetAmmo(hull->GetMaxAmmo());ship->SetFuel(hull->GetMaxFuel());ship->SetEnergy(hull->GetMaxEnergy());
    ShipSoundNotifications alerts;std::vector<int> events;
    auto update=[&]{alerts.Update(ship,[&](SoundID id){events.push_back(id);});};
    update();require(events.empty(),"Full ship emitted low-resource warnings");
    ship->SetFraction(.14f);ship->SetAmmo(0);ship->SetFuel(0);ship->SetEnergy(0);update();
    require(events==std::vector<int>{hullLowSound,ammoLowSound,fuelLowSound,energyLowSound},"Original warning thresholds/order changed");
    events.clear();update();require(events.empty(),"Steady low resources repeated warnings");
    auto* booster=ship->CreateAndAddPart(fixture.types(ET_Afterburner,ship).front(),0,1);require(booster,"No booster for empty-fuel warning");booster->SetMountedFraction(1);
    ship->SetStateBits(afterburnerButtonIGC,afterburnerButtonIGC);update();require(events==std::vector<int>{salNoFuelSound},"Empty booster did not emit its transition warning");
    events.clear();update();require(events.empty(),"Held empty booster repeated warning");
    ship->SetStateBits(afterburnerButtonIGC,0);update();ship->SetStateBits(afterburnerButtonIGC,afterburnerButtonIGC);update();require(events==std::vector<int>{salNoFuelSound},"Re-triggered empty booster lost warning");
    require(ShipImpactSound(true,true)==myShieldHitSound && ShipImpactSound(false,false)==otherHullHitSound,"Impact ownership/shield selection wrong");
    require(ExplosionSound(10)==smallExplosionSound && ExplosionSound(100)==mediumExplosionSound && ExplosionSound(101)==largeExplosionSound,"Explosion threshold boundaries changed");
}
void framing() {
    for(bool explicitFocus:{false,true})for(float minimum:{25.f,700.f,15000.f})for(float distance:{50.f,6000.f,20000.f}){
        Vector focus(4000,-1200,700),fallback(-200,640,88),center(17,55,33),displacement(6,3,1);
        // Frozen WinTrek::PositionCommandView position/distance branch.
        auto expectedCenter=explicitFocus?focus:fallback;float expectedDistance=explicitFocus?10000.f:distance;
        if(!explicitFocus && expectedDistance<minimum)expectedDistance=minimum;
        PositionCommandView(explicitFocus?&focus:nullptr,fallback,minimum,center,displacement,distance);
        near(center,expectedCenter,"original explicit/default command focus");near(displacement,Vector::GetZero(),"original focus resets command pan displacement");
        near(distance,expectedDistance,"original focus preserves default zoom and clamps station/ship minimum");
    }
    for(float dt:{0.f,.001f,1.f/165,.1f,.4f})for(bool out:{false,true})for(int i=0;i<=200;++i){
        // Frozen WinTrek UpdateJoystick/OnTrekKey equations, including their
        // direction-specific bounds (an initially wide FOV is not snapped).
        float fov=RadiansFromDegrees(float(i)),expected=fov;
        if(out){expected+=dt;if(expected>RadiansFromDegrees(50.f))expected=RadiansFromDegrees(50.f);}
        else{expected-=dt;if(expected<RadiansFromDegrees(5.f))expected=RadiansFromDegrees(5.f);}
        near(ZoomFieldOfView(fov,dt,out,RadiansFromDegrees(5.f),RadiansFromDegrees(50.f)),expected,"original cockpit held/wheel zoom");
        float distance=i*50.f;expected=distance;
        if(out){expected+=dt*expected;if(expected>6000)expected=6000;}
        else{expected-=dt*expected;if(expected<100)expected=100;}
        near(ZoomCameraDistance(distance,dt,out,100,6000),expected,"original proportional external held/wheel zoom");
        expected=distance;
        if(out){expected+=dt*expected*2;if(expected>20000)expected=20000;}
        else{expected-=dt*expected*2;if(expected<50)expected=50;}
        near(ZoomCameraDistance(distance,dt,out,50,20000,2),expected,"original proportional command wheel zoom");
    }
    for(int view=0;view<2;++view)for(int angle=0;angle<32;++angle)for(int dx=-5;dx<=5;++dx)for(int dy=-5;dy<=5;++dy) {
        Orientation o=CommandViewOrientation(view!=0,angle*(2*pi/32));
        Vector center(73,-451,133),displacement(6,4,12),expected=center,expectedDisplacement=displacement;
        float right=dx*171.f,down=dy*121.f;
        if(right){Vector v=o.GetRight()*right;expected+=v;expectedDisplacement+=v;}
        if(down){Vector up=o.GetUp();up.z=0;float length=float(std::sqrt(up.x*up.x+up.y*up.y));Vector v=up*(down/length);expected-=v;expectedDisplacement-=v;}
        PanCommandView(o,right,down,displacement,center);near(center,expected,"original command-view plane movement");near(displacement,expectedDisplacement,"original command-view tracking offset");
        Vector far=center*100;float l2=far.LengthSquared();Vector clamped=far;if(l2>4.f*10000.f*10000.f)clamped*=20000.f/float(std::sqrt(l2));ClampCommandView(far);near(far,clamped,"original command-view displacement bound");
    }
    for(Vector axis:{Vector(30,4,-7),Vector(0,100,0),Vector(-50,2,1)}) {
        Orientation actual,expected;actual.Yaw(.4f);actual.Roll(.2f);expected=actual;
        Vector origin(2,5,-9),position;
        Vector direction=axis.Normalize(),up=expected.GetUp();
        if(std::fabs(up*direction)>.95f)up=expected.GetRight();
        Vector reference=origin-direction*24.f+CrossProduct(direction,up)*3.f;
        expected.TurnTo(direction);TrackCamera(origin,origin+axis,12,actual,position);
        near(position,reference,"Original tracking position changed");near(actual.GetForward(),expected.GetForward(),"Original tracking orientation changed");
    }
    Orientation orientation;Vector position;TrackCamera(Vector::GetZero(),Vector::GetZero(),5,orientation,position);
    require(std::isfinite(position.x) && std::isfinite(orientation.GetForward().z),"Coincident tracking target made the camera nonfinite");
    orientation.Yaw(.7f);orientation.Pitch(-.2f);Vector center;float offset;
    const Vector ship(10,20,30),target(-100,70,-80);
    FrameFlyBy(ship,target,orientation,center,offset);near(center,.5f*(ship+target),"Flyby midpoint changed");
    Vector delta=target-center;float back=delta*orientation.GetBackward();
    float expected=1.25f*(3.f*std::sqrt(delta.LengthSquared()+back*back)+std::fabs(back));
    near(offset,expected,"Original conservative flyby extent changed");
    Orientation orbit=orientation;orbit.Pitch(.035f);orbit.Yaw(.02f);orbit.Roll(.05f);OrbitCamera(orientation,.1f);
    near(orientation.GetForward(),orbit.GetForward(),"Original orbit angular rates changed");
}
void viewCommandGates(const std::filesystem::path& artwork) {
    using namespace Native;SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    auto loader=[](auto&,auto&){return ModelHandle{1,1};};
    Game authority(artwork,loader,0,0,options,{},GameRole::Authority),replica(artwork,loader,0,0,options,{},GameRole::Replica);
    std::vector<Net::Action> actions;replica.networkActions([&](const Net::Action& a){actions.push_back(a);return true;});
    require(actions.size()==1 && actions.front().op==Net::Operation::ChatPreferences,"replica startup does not synchronize its existing chat preferences");actions.clear();
    replica.commandView(true);require(!replica.viewCamera(CameraMode::Orbit) && actions.empty(),"unassigned camera input sends authority view actions before local admission");
    int id=authority.playerShip()->GetObjectID();authority.beginNetworkPublication();replica.applyNetworkFrame(authority.networkFrame(id,0,0));
    require(replica.state().docked,"camera admission fixture reaches the original hangar");
    replica.commandView(true);require(actions.size()==1 && actions.back().args[1]==1,"valid command view does not send its authority action");
    actions.clear();require(!replica.viewSector(-1) && !replica.viewSector(SHRT_MAX+1) && !replica.viewSector(65536) && actions.empty(),"invalid sector input wraps to an IGC ID or sends a rejected network action");
    require(!replica.viewCamera(CameraMode::Command) && !replica.viewCamera(CameraMode::Count) && actions.empty(),"invalid camera mode leaves command view on the authority");
    replica.commandView(false);require(actions.size()==1 && !replica.state().commandView,"valid combat view fails to synchronize with authority");actions.clear();
    authority.launch();authority.beginNetworkPublication();replica.applyNetworkFrame(authority.networkFrame(id,1,0));
    require(replica.state().cinematic,"camera gate fixture receives original launch override");
    replica.commandView(true);require(!replica.viewCamera(CameraMode::Orbit) && actions.empty(),"launch camera input sends a view action despite the original override gate");
    puts("Camera command dispatch: unassigned/launch gates, valid command/combat actions, invalid modes and bounded non-wrapping sector IDs passed");
}
void cinematics(const std::filesystem::path& artwork) {
    using namespace Native;Assets assets(artwork);SessionOptions options;options.scenario=Scenario::FreeFlight;options.enemies=0;options.startInHangar=true;
    Game game(artwork,[&](auto& name,auto&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* ship=game.playerShip();Vector station=ship->GetStation()->GetPosition();game.launch();
    require(game.state().cinematic && !game.state().cinematicDeath,"launch must enter the original three-second override view");
    require(!game.viewCamera(CameraMode::Orbit),"cinematic view accepts a camera key during the override");
    auto reference=[&](Vector from,Vector toward,float length,Orientation& o){
        Vector direction=(toward-from).Normalize(),up=o.GetUp();if(std::fabs(up*direction)>.95f)up=o.GetRight();
        Vector position=from-direction*(length*2.f)+CrossProduct(direction,up)*(length*.25f);o.TurnTo(direction);return position;
    };
    Orientation o(Vector(1,0,0),Vector(0,0,1));auto expected=reference(ship->GetPosition(),station,ship->GetRadius()*3,o);
    CameraInput input;auto launch=game.camera(input);near(launch.position,{expected.x,expected.y,expected.z},"original launch framing");
    near(launch.forward,{o.GetForward().x,o.GetForward().y,o.GetForward().z},"original launch heading");require(launch.showPlayer,"launch frame hides the launching ship");
    FlightInput thrust;thrust.throttle=1;
    for(int i=0;i<29;++i){game.step(thrust,.1);game.camera(input);}
    require(game.state().cinematic,"launch override ended before three seconds");
    auto lastLaunch=game.camera(input);
    for(int i=0;i<3;++i)game.step(thrust,.1);
    require(!game.state().cinematic,"launch override did not return to the cockpit");
    auto entering=game.camera(input);
    require(entering.animating && entering.showPlayer,"launch exit cuts to the cockpit and prematurely hides the player's ship");
    float blend=.5f-.5f*std::cos(pi*.3f);
    near(entering.position,lastLaunch.position*(1-blend)+game.state().cockpit*blend,"launch exit lost the original one-second cosine position transition");
    auto external=game.scene(true).size();require(ship->GetVisibleF() && external==game.scene(false).size()+1,"launch exit transition omits the player's hull from scene submission");
    for(int i=0;i<8;++i){game.step(thrust,.1);game.camera(input);}
    auto cockpit=game.camera(input);require(!cockpit.animating && !cockpit.showPlayer,"settled cockpit retains the external hull or transition");
    near(cockpit.position,game.state().cockpit,"launch camera returned to a stale cockpit");
    Vector from=ship->GetPosition(),toward=from+ship->GetOrientation().GetBackward()*(ship->GetRadius()*2);float radius=ship->GetRadius();
    o=Orientation(Vector(1,0,0),Vector(0,0,1));expected=reference(from,toward,(radius*2)*3,o);
    int id=ship->GetObjectID();game.eject();auto death=game.camera(input);
    require(game.state().cinematic && game.state().cinematicDeath && game.state().pod && game.state().shipId==id,"ejection must keep its pilot identity and enter the death camera");
    near(death.position,{expected.x,expected.y,expected.z},"death framing uses the pre-ejection hull and position");require(!death.showPlayer,"death camera displays the new pod before its return");
    thrust.yaw=thrust.pitch=1;thrust.boost=thrust.fire=true;input.headYaw=pi;
    for(int i=0;i<49;++i){game.step(thrust,.1);auto pose=game.camera(input);near(pose.position,death.position,"death camera moves with the replacement pod");near(pose.forward,death.forward,"head look changes the frozen death camera");}
    require(game.state().cinematic,"death override ended before five seconds");
    for(int i=0;i<3;++i)game.step({},.1);
    require(!game.state().cinematic && game.state().pod,"death override did not return control to the same pod");
    // Leave an active override at teardown; its borrowed cluster must be released
    // before the IGC mission is destroyed (the sanitizer suite covers ownership).
    game.eject();
    Game chaseGame(artwork,[&](auto& name,auto&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    input.chase=true;chaseGame.launch();chaseGame.camera(input);
    for(int i=0;i<29;++i){chaseGame.step(thrust,.1);chaseGame.camera(input);}
    for(int i=0;i<3;++i)chaseGame.step(thrust,.1);
    auto chaseEntering=chaseGame.camera(input);require(!chaseGame.state().cinematic && chaseEntering.animating && chaseEntering.showPlayer,"preferred chase launch loses the original external transition");
    for(int i=0;i<8;++i){chaseGame.step(thrust,.1);chaseGame.camera(input);}
    require(chaseGame.camera(input).animating,"preferred chase launch incorrectly uses the one-second internal transition");
    for(int i=0;i<10;++i){chaseGame.step(thrust,.1);chaseGame.camera(input);}
    require(!chaseGame.camera(input).animating && chaseGame.camera(input).showPlayer,"preferred chase does not settle after its two-second external transition");
    puts("Original launch/death framing, duration, one-second cockpit/two-second chase transitions, hull visibility, input gating and pilot identity passed");
}
void sharedBehavior()
{
    const float fov = RadiansFromDegrees(50);
    CameraMotion camera(fov);
    int draws = 0;
    auto random = [&](float low, float high) { return ++draws % 2 ? high : low; };
    camera.SetJiggle(4);
    camera.Apply(1, true, true, .5f, 100, 50, random);
    Orientation expected;
    expected.Yaw(RadiansFromDegrees(.5f) * 1.5f);
    expected.Pitch(-RadiansFromDegrees(.5f) * 1.5f);
    near(camera.GetViewOrientation().GetForward(), expected.GetForward(),
         "Original boost/impact amplitude or decay changed");
    require(draws == 2, "Shake sampled RNG incorrectly");
    camera.Apply(0, false, true, 1, -10, 50, random);
    near(camera.GetViewOrientation().GetForward(), Vector(0, 0, -1), "External view shook");
    near(camera.GetViewFOV(), fov, "External view got aleph FOV");
    camera.SetJiggle(.5f);
    camera.Apply(0, true, false, 0, 25, 50, random);
    require(draws == 2, "No-afterburner residual threshold changed");
    near(camera.GetViewFOV(), fov * .5f, "Aleph compression changed");
    camera.Apply(0, true, false, 0, -10, 50, random);
    near(camera.GetViewFOV(), .1f, "Aleph FOV lower bound changed");
    camera.SetHeadOrientation(pi);
    camera.Apply(0, false, false, 0, 0, 0, random);
    near(camera.GetViewOrientation().GetForward(), Vector(0, 0, 1), "Rear look basis is wrong");
    near(camera.GetOrientation().GetForward(), Vector(0, 0, -1), "Head look changed ship basis");
    camera.SetHeadOrientation(0);
    camera.SetAnimation(1);
    camera.SetPosition(Vector(10, 20, 30));
    Orientation destination;
    destination.Yaw(pi * .5f);
    camera.SetOrientation(destination);
    camera.SetFOV(fov * .5f);
    require(camera.Apply(.5f, false, false, 0, 0, 0, random), "View transition ended early");
    near(camera.GetPosition(), Vector(5, 10, 15), "Cosine transition midpoint changed");
    Orientation midway;
    midway.Yaw(pi * .25f);
    near(camera.GetOrientation().GetForward(), midway.GetForward(), "Slerp transition changed");
    near(camera.GetViewFOV(), fov * .75f, "FOV interpolation changed");
    camera.SetAnimation(0);
    near(camera.GetOrientation().GetForward(), destination.GetForward(),
         "Cancelling transition left stale orientation");
    near(camera.GetPosition(), Vector(10, 20, 30), "Cancelling transition left stale position");
    near(Vector(1,0,0)*camera.GetTargetOrientation(camera.GetPosition()+Vector(10,0,0)),Vector(0,0,-1),
         "HUD preview did not turn toward the actual target");
    camera.SetAnimation(1);
    camera.SetAnimatePosition(false);
    camera.SetPosition(Vector(9, 8, 7));
    camera.Apply(.25f, false, false, 0, 0, 0, random);
    near(camera.GetPosition(), Vector(9, 8, 7), "Internal position was delayed");

    for (int hz : {30, 165, 1000})
    {
        ChaseMotion chase;
        for (int i = 0; i <= hz * 4; ++i)
        {
            double t = double(i) / hz;
            chase.Record(t, Vector(float(t), 2, -3));
            // Simulate repeated Time ticks; there must be no extra integration.
            chase.Record(t, Vector(float(t), 2, -3));
        }
        float delay = .383f;
        near(chase.Displacement(delay), Vector(4 * delay - delay * delay * .5f, 2 * delay, -3 * delay),
             "Chase integration depends on refresh or ring wrap");
        chase.Record(0, Vector(1, 1, 1));
        near(chase.Displacement(.2f), Vector::GetZero(), "Rewinding kept stale chase samples");
    }
    ChaseMotion chase;
    Orientation orientation;
    Vector position;
    chase.Compute(Orientation(), Vector(1, 2, 3), Vector(0, 0, -100), 10, 100, Orientation(), 500,
                  orientation, position);
    float zoom = (500.f - 100) / 59;
    near(position, Vector(1, 2 + 3.333f, 3 + 50 + zoom + 15),
         "Original velocity-dependent chase placement changed");
    ShipSoundLevels sound;
    sound.Update(0, 0, 0, true);
    sound.Update(1, 1, .1f, false);
    near(sound.thrust, .05f, "Original thrust audio slew changed");
    near(sound.turn, .2f, "Original turn audio slew changed");
    near(sound.MainGain(true), -60 * std::pow(.95f, 64), "Original cockpit engine gain curve changed",
         .0001f);
    near(sound.TurnGain(true), -38.4f, "Original maneuver gain curve changed");
    near(sound.MainPitch(), .525f, "Original engine pitch changed");
}
void gameBehavior(const std::filesystem::path &artwork)
{
    using namespace Native;
    Assets assets(artwork);
    auto loader = [&](const std::string &name, const std::string &) {
        auto m = assets.model(name);
        return ModelHandle{0, m->radius, m->frames};
    };
    SessionOptions options;
    options.scenario = Scenario::FreeFlight;
    options.enemies = 0;
    Game game(assets.root(), loader, 0, 0, options);
    CameraInput input;
    auto idle = game.camera(input);
    near(idle.position, game.state().cockpit, "Cockpit offset lost");
    near(idle.forward, game.state().forward, "Unpowered camera altered orientation");
    input.fieldOfView=DegreesFromRadians(ZoomFieldOfView(RadiansFromDegrees(input.fieldOfView),.1f,false,RadiansFromDegrees(5.f),RadiansFromDegrees(input.fieldOfView)));
    near(game.camera(input).fieldOfView,input.fieldOfView,"cockpit zoom reaches the actual native projection");
    near(game.state().viewFov,input.fieldOfView,"cockpit zoom reaches the IGC steering snapshot");input.fieldOfView=65;
    require(!game.zoomCommandCamera(false),"command zoom rejects an inactive command view");
    game.commandView(true);game.camera(input);
    // The original two-second command entrance now blends the displayed pose.
    // Measure a wheel detent after it settles, independently of that blend.
    for(int i=0;i<332;++i){game.step({},1./165);game.camera(input);}
    require(!game.camera(input).animating,"original command camera entrance does not settle");
    const auto beforeZoom=game.camera(input).position;
    require(game.zoomCommandCamera(false),"original command wheel zoom accepts a command view");const auto afterZoom=game.camera(input).position;
    near(glm::length(afterZoom-game.state().position)/glm::length(beforeZoom-game.state().position),.8f,"command wheel zoom uses its fixed detent interval instead of frame time",.002f);
    require(!game.zoomCommandCamera(true,-.1f) && !game.zoomCommandCamera(true,2),"invalid command zoom intervals are inert");
    game.commandView(false);game.camera(input);
    FlightInput flight;
    flight.throttle = 1;
    flight.boost = true;
    bool shook = false, sounded = false;
    for (int i = 0; i < 165; ++i)
    {
        game.step(flight, 1. / 165);
        auto camera = game.camera(input);
        shook |= glm::length(camera.forward - game.state().forward) > 1e-5f;
        auto sounds = game.shipSounds(true);
        sounded |= sounds.size() == 5 && sounds[3].id >= 0;
        auto world=game.worldSounds(true);bool inside=false,outside=false;
        for(const auto& sound:world){inside|=sound.relative && sound.gain>0;outside|=!sound.relative;require(std::isfinite(sound.position.x) && std::isfinite(sound.direction.x),"Sound attachment became nonfinite");}
        require(inside && outside,"World audio omitted cockpit or nearby ambience");game.clearSounds();
        auto frozen = game.camera(input);
        near(camera.forward, frozen.forward, "Paused camera resampled shake");
    }
    require(shook && sounded && game.state().fuel < 1,
            "IGC afterburner power did not drive camera and audio");
    flight.boost = false;
    flight.throttle = .25f;
    for (int i = 0; i < 500; ++i)
    {
        game.step(flight, 1. / 165);
        game.camera(input);
    }
    near(game.camera(input).forward, game.state().forward, "Released booster kept shaking");
    require(game.shipSounds(true)[3].id < 0, "Released booster kept its sound");
    flight.yaw = .2f;
    flight.pitch = -.15f;
    flight.roll = .1f;
    for (int i = 0; i < 25; ++i)
    {
        game.step(flight, 1. / 165);
        near(game.camera(input).forward, game.state().forward, "Cockpit steering was delayed");
    }
    input.chase = true;
    auto transition = game.camera(input);
    require(transition.animating && transition.showPlayer, "Entering chase failed to animate/show ship");
    for (int i = 0; i < 331; ++i)
    {
        game.step(flight, 1. / 165);
        game.camera(input);
    }
    auto chase = game.camera(input);
    require(!chase.animating && chase.showPlayer, "Chase transition did not finish");
    require(glm::distance(chase.position, game.state().position) > 30, "Original chase distance missing");
    require(glm::length(chase.forward - game.state().forward) > 1e-4f, "Chase turn lag missing");
    require(!game.shipSounds(false)[0].relative, "Exterior sound did not use a world position");
    input.chase = false;
    require(game.camera(input).showPlayer, "Returning to cockpit hid ship before transition finished");
    for (int i = 0; i < 166; ++i)
    {
        game.step({}, 1. / 165);
        game.camera(input);
    }
    require(!game.camera(input).showPlayer && !game.camera(input).animating,
            "Cockpit transition did not finish");
    input.headYaw = pi;
    near(game.camera(input).forward, -game.state().forward, "In-game rear view is not rear-facing");
    input.headYaw = 0;
    game.respawn();
    auto reset = game.camera(input);
    require(!reset.animating, "Respawn retained old view animation");
    near(reset.position, game.state().cockpit, "Respawn camera remained in old position");

    game.nextTarget();
    for(CameraMode mode:{CameraMode::External,CameraMode::Target,CameraMode::FlyBy,CameraMode::Orbit,CameraMode::Station,CameraMode::Missile}) {
        input.mode=mode;input.chase=false;game.camera(input);
        for(int i=0;i<332;++i){game.step({},1./165);game.camera(input);}
        const auto& pose=game.camera(input);
        require(pose.mode==mode && pose.showPlayer && !pose.animating,"External camera mode did not settle");
        require(std::isfinite(pose.position.x) && std::isfinite(pose.forward.x),"External camera produced nonfinite geometry");
        near(glm::length(pose.forward),1,"External camera heading lost normalization");
        near(glm::dot(pose.forward,pose.up),0,"External camera basis lost orthogonality");
    }
    input.mode=CameraMode::Cockpit;game.respawn();game.camera(input);

    // Drive into an actual IGC asteroid without boost. Only PlayVisualEffect's
    // collision callback can introduce shake in this case.
    glm::vec3 rock{};
    float nearest = 1e30f;
    for (auto &c : game.contacts())
        if (c.type == ContactAsteroid)
        {
            float distance = glm::distance(c.position, game.state().position);
            if (distance < nearest)
            {
                nearest = distance;
                rock = c.position;
            }
        }
    require(nearest < 1e30f, "Collision test has no asteroid");
    bool impact = false;
    for (int i = 0; i < 20000 && !impact && !game.state().dead; ++i)
    {
        auto direction = glm::normalize(rock - game.state().position);
        FlightInput steer;
        steer.yaw = std::clamp(-glm::dot(direction, game.state().right) * 3, -1.f, 1.f);
        steer.pitch = std::clamp(-glm::dot(direction, game.state().up) * 3, -1.f, 1.f);
        steer.throttle = glm::dot(direction, game.state().forward) > .9f ? .4f : 0;
        game.step(steer, 1. / 165);
        if (!game.state().dead)
            impact = glm::length(game.camera(input).forward - game.state().forward) > 1e-5f;
    }
    require(impact, "Real IGC collision did not reach camera effect callback");
    puts("IGC boost/release, collision shake, camera modes, pause, head look, and engine loops passed");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        soundPolicy(argc > 1 ? std::filesystem::path(argv[1]) : Native::Assets::steamArtwork());
        framing();
        viewCommandGates(argc > 1 ? std::filesystem::path(argv[1]) : Native::Assets::steamArtwork());
        sharedBehavior();cinematics(argc > 1 ? std::filesystem::path(argv[1]) : Native::Assets::steamArtwork());
        gameBehavior(argc > 1 ? std::filesystem::path(argv[1]) : Native::Assets::steamArtwork());
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "Camera check: %s\n", e.what());
        return 1;
    }
}
