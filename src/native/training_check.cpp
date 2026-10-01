#include "training_host.hpp"
#undef trekClient
#include "game.hpp"
#include "audio.hpp"
#include "training_slides.hpp"
#include "../training/Training.h"
#include "../training/Clock.h"
#include "../training/playsoundaction.h"
#include "../training/GetKeyCondition.h"
#include "../training/GetChatCondition.h"
#include "../training/objectpointingatcondition.h"
#include "../training/trainingmission.h"
#include "../ClientCore/CameraFraming.h"
#include "test_reference/image_text.hpp"
namespace Training { extern TrainingMission* g_pMission; }
#include <cstdio>
#include <stdexcept>
#include <set>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv) {
    try {
        setvbuf(stdout,nullptr,_IOLBF,BUFSIZ);
        Native::Assets assets(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());Native::Audio audio(assets,true);
        Native::QuickChatCatalog quickChat(assets);audio.preload(quickChat);
        size_t slideCount=0;
        size_t texts=0,keyedPixels=0;
        for(int id:{1,2,3,4,5,6,8,10})for(bool post:{false,true}) {
            if(id==1 && !post)continue; // Introduction is itself a post-training slideshow.
            Native::TrainingSlides slides(assets,audio,id,post);require(!slides.finished(),"authored slide sequence is empty");
            for(auto& slide:slides.all())for(auto& c:slide.view.artwork){
                require(slide.view.original && slide.view.canvas==glm::ivec2(800,600),"training slide is not an original authored 800 x 600 image");
                if(c.image)require(c.image->width>0,"authored slide image missing");
                if(c.kind==Native::OriginalControl::Kind::Text){
                    require(c.font=="largeFont" && c.textMode==Native::OriginalControl::TextMode::Image && c.color==glm::vec4(1) && c.bounds.x==150 && c.bounds.y==150 && c.bounds.z==500,"installed training text loses authored font/color/width/image coordinates");
                    auto font=assets.font("largeFont");ReferenceImageText::Font metrics;for(int i=0;i<256;++i)metrics.advances[i]=font->glyphs[i].z;
                    ReferenceImageText::StringImage original{&metrics};ReferenceImageText::ZString remaining{c.text};size_t rows=0;
                    while(!remaining.text.empty()){original.BreakLine(remaining,500);++rows;}
                    require(c.bounds.w==float(rows*font->height),"training slide height differs from frozen original font/wrapping");++texts;
                }
            }
            size_t decoded=audio.decodedWaveCount();slideCount+=slides.all().size();
            if(id==2 && !post){auto& artwork=slides.current().view.artwork;
                require(artwork.size()==2 && artwork.back().bounds==glm::vec4(360,600-285-artwork.back().image->height,artwork.back().image->width,artwork.back().image->height) && !artwork.back().additive,"original slideshow layers/translation/color-key blending");
                auto plain=assets.image("tm_slide_circle_overlaybmp");auto keyed=artwork.back().image;
                for(size_t i=0;i<plain->pixels.size();i+=4)if(!plain->pixels[i] && !plain->pixels[i+1] && !plain->pixels[i+2]){require(!keyed->pixels[i+3],"training circle black texel is not color keyed");++keyedPixels;}
                for(int i=0;i<2400 && !slides.finished();++i){audio.tick(.05);slides.tick(.05);}require(slides.finished(),"slide narration did not automatically advance after its gap");}
            else while(!slides.finished()){audio.tick(.01);slides.advance();}
            auto* last=&slides.all().back();require(&slides.current()==last,"completed slideshow loses its final image during the original startup delay");
            require(audio.decodedWaveCount()==decoded,"slide decoded narration during playback");
        }
        require(texts>=10 && keyedPixels>100,"training artwork audit misses text or keyed overlays");
        printf("%zu authored introduction/debrief slides: shared image compilation, %zu authored text/frozen wrapping checks, %zu keyed pixels, layers, narration, manual advance and retained completion image passed\n",slideCount,texts,keyedPixels);
        int combatRuns=0,repairRuns=0;std::array<int,11> arenaRuns{};
        for(int id:{2,3,4,5,6,8,10,4,2,6,10,8}) {
            if(argc>2 && id!=std::atoi(argv[2]))continue;
            audio.stopAll();Native::SessionOptions options;options.trainingMission=id;
            Native::Game game(assets.root(),[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
            game.startTraining(audio);require(Training::IsTraining() && Training::GetTrainingMissionID()==id,"authored mission did not start");
            require(game.core()->GetLastUpdate().clock()==1000u,"offline training clock must start at the match epoch independently of host uptime");
            auto* ship=game.playerShip();require(ship && ship->GetSide()->GetObjectID()==0 && game.core()->GetClusters()->n()>0,"authored mission world/player setup");
            require(ship->GetSide()->GetCivilization()->GetObjectID()==(id==8?201:25),"mission civilization leaks across runs");
            require(game.state().trainingMission==id && !game.state().complete,"native training presentation snapshot");
            auto start=Training::Clock::Now();game.recordKey(TK_PauseTM);game.step({},.1);
            require(Training::Clock::Now()==start && game.state().trainingPaused,"training pause freezes simulation clock");
            game.recordKey(TK_PauseTM);game.step({},.05);audio.tick(.05);
            require(Training::Clock::Now()>start && !game.state().trainingPaused,"training resumes simulation clock");
            if(id==2 || id==3) {
                Native::FlightInput input;input.yaw=input.pitch=input.roll=input.throttle=1;input.fire=input.boost=true;
                game.step(input,.05);audio.tick(.05);
                auto& control=ship->GetControls();require(control.jsValues[0]==0 && control.jsValues[1]==0 && control.jsValues[2]==0 && control.jsValues[3]==-1 && !(ship->GetStateM()&(weaponsMaskIGC|afterburnerButtonIGC)),"authored introductory input constraints are applied to IGC");
                require(!game.recordKey(TK_ToggleAutoPilot) && !game.recordKey(TK_RejectCommand),"authored training input restrictions");
            }
            float throttle=1;bool forced=false;require(game.takeThrottleReset(throttle,&forced) && forced && throttle==0,"authored starting throttle overrides a physical throttle");
            size_t decoded=audio.decodedWaveCount();
            for(int i=0;i<600;++i){game.step({},.05);audio.tick(.05);game.camera({});game.scene(false);}
            require(audio.decodedWaveCount()==decoded,"training narration decoded during playback");
            require(std::isfinite(game.state().position.x) && game.state().hull>0,"authored mission startup simulation stays valid");
            if(id==2) {
                require(Native::trainingHost->camera==Native::TrainingHost::cmCockpit,"real narration completion advances authored camera action");
                for(int i=0;i<2400 && game.state().trainingOverlay==NoTrainingOverlay;++i){game.step({},.05);audio.tick(.05);game.camera({});}
                require(game.state().trainingOverlay==RollTrainingOverlay,"original lesson action publishes its actual roll-control highlight to the native HUD");
            }
            Training::PlaySoundAction silent(NA);silent.Execute();silent.Stop();require(silent.HasStopped(),"silent training cue completion");
            if((id==6 || id==10) && arenaRuns[id]==0) {
                require(game.returnToBase(),"arena return-to-base action");
                for(int frame=0;frame<6000 && !game.state().docked && !game.state().complete;++frame){game.step({},.02);audio.tick(.02);}
                require(game.state().docked,"original arena must dock through autopilot/collision");
                auto hulls=game.state().hullChoices;require(hulls.size()>6,"authored technology exposes more than practice's fixed six hulls");
                for(auto& hull:hulls)require(ship->GetStation()->CanBuy(game.core()->GetHullType(hull.id)),"training hull menu exposes unavailable technology");
                require(game.buyHull(481) && ship->GetBaseHullType()->GetObjectID()==481,"buy original authored heavy interceptor loadout");
                auto loadout=game.currentLoadout();require(!loadout.parts.empty() && game.applyLoadout(loadout),"training saved loadout uses original station purchase policy");
                for(auto& part:game.partChoices(Native::PartKind::Weapon,0))require(ship->GetStation()->CanBuy(game.core()->GetPartType(part.id)),"training part menu exposes unavailable technology");
                game.launch();require(ship->GetCluster() && !game.state().docked,"original arena launches refitted hull");
                printf("Original mission %d: real return/dock, authored hull/part catalog, station refit, saved loadout and launch passed.\n",id);
            }
            if((id==6 || id==10) && arenaRuns[id]<1 && argc>3 && std::string_view(argv[3])=="--arena-input") {
                std::set<HullID> hulls;int lastKills=0;bool congratulated=false,servicing=false,refitted=false;
                for(int frame=0;frame<400000 && !game.state().complete;++frame) {
                    Native::FlightInput input;auto& state=game.state();
                    if(state.docked) {
                        if(!refitted) {
                            auto choice=std::find_if(state.hullChoices.begin(),state.hullChoices.end(),[](auto& hull){return hull.id==488;});
                            require(choice!=state.hullChoices.end() && game.buyHull(choice->id),"original arena station permits its authored advanced fighter");
                            int bestGun=-1;float bestPower=0;
                            for(auto& part:game.partChoices(Native::PartKind::Weapon,0)){
                                auto* data=static_cast<const DataWeaponTypeIGC*>(game.core()->GetPartType(part.id)->GetData());
                                auto* projectile=game.core()->GetProjectileType(data->projectileTypeID);float power=projectile->GetPower()/data->dtimeBurst;
                                float demand=data->energyPerShot/data->dtimeBurst*ship->GetHullType()->GetMaxFixedWeapons();
                                if(demand>0)power*=std::min(1.f,ship->GetHullType()->GetRechargeRate()*.5f/demand);
                                if(power>bestPower){bestPower=power;bestGun=part.id;}
                            }
                            require(bestGun>=0,"arena station has a legal damaging gun");
                            for(Mount mount=0;mount<ship->GetHullType()->GetMaxFixedWeapons();++mount)require(game.replacePart(Native::PartKind::Weapon,mount,bestGun),"arena pilot fits legal station guns");
                            int bestShield=-1;float strength=0;
                            for(auto& part:game.partChoices(Native::PartKind::Shield,0)){
                                auto* data=static_cast<const DataShieldTypeIGC*>(game.core()->GetPartType(part.id)->GetData());if(data->maxStrength>strength){strength=data->maxStrength;bestShield=part.id;}
                            }
                            require(bestShield>=0 && game.replacePart(Native::PartKind::Shield,0,bestShield),"arena pilot fits legal station shield");
                            printf("Arena %d pilot: %s, %s (%g sustained DPS) x %d, %s (%g shield)\n",id,ship->GetHullType()->GetName(),game.core()->GetPartType(bestGun)->GetName(),bestPower,ship->GetHullType()->GetMaxFixedWeapons(),game.core()->GetPartType(bestShield)->GetName(),strength);
                            if(game.state().selectedWeapon!=-1)game.selectWeapon(-1);
                            require(game.state().selectedWeapon==-1,"arena pilot fires all fitted fixed weapons as a group");refitted=true;
                        }
                        game.launch();servicing=false;
                    }
                    auto* primary=static_cast<IweaponIGC*>(ship->GetMountedPart(ET_Weapon,0));
                    if(!state.cinematic && ship->GetCluster() && !servicing && (!refitted || (primary && primary->GetAmmoPerShot()>0 && state.ammoFraction<.4f) || state.hull<.9f))servicing=true;
                    if(!servicing && !state.cinematic && ship->GetCluster()) {
                        IshipIGC* target=nullptr;float priority=FLT_MAX;
                        for(auto& contact:game.contacts())if(contact.enemy && contact.type==OT_ship) {
                            auto* candidate=game.core()->GetShip(ShipID(contact.id));if(!candidate || !candidate->GetBaseHullType())continue;
                            auto* weapon=static_cast<IweaponIGC*>(candidate->GetMountedPart(ET_Weapon,0));
                            float role=weapon && weapon->GetProjectileType()->GetPower()<0?0:candidate->GetHullType()->HasCapability(c_habmFighter)?2000000:1000000;
                            float score=role+(candidate->GetObjectID()==state.targetId?-100000.f:(candidate->GetPosition()-ship->GetPosition()).Length());
                            if(score<priority){priority=score;target=candidate;}
                        }
                        if(target && state.targetId!=target->GetObjectID())game.selectTarget(OT_ship,target->GetObjectID());
                        if(target && target->GetCluster()==ship->GetCluster()) {
                            hulls.insert(target->GetBaseHullType()->GetObjectID());
                            auto* gun=static_cast<IweaponIGC*>(ship->GetMountedPart(ET_Weapon,0));require(gun,"arena pilot retains its original gun");
                            Vector direction=(target->GetPosition()-ship->GetPosition()).Normalize();float impact=solveForLead(ship,target,gun,&direction,1);
                            ControlData controls;turnToFace(direction,.02f,ship,&controls,1);
                            input.yaw=controls.jsValues[c_axisYaw];input.pitch=controls.jsValues[c_axisPitch];input.roll=controls.jsValues[c_axisRoll];
                            float distance=(target->GetPosition()-ship->GetPosition()).Length(),range=gun->GetLifespan()*gun->GetProjectileType()->GetSpeed();
                            float desired=std::max(ship->GetRadius()+target->GetRadius()+60,range*.55f);
                            float speed=target->GetVelocity()*ship->GetOrientation().GetForward()+(distance-desired)*.3f;
                            input.throttle=std::clamp(speed/ship->GetHullType()->GetMaxSpeed(),0.f,1.f);input.backward=distance<desired-20;
                            float facing=direction*ship->GetOrientation().GetForward();input.boost=distance>range*2.f && facing>.99f && state.fuel>.5f;
                            input.fire=impact<=gun->GetLifespan()*.95f && facing>.999f;input.chaff=state.hull<.95f && frame%150==0;input.missile=state.missileLock>.9f && facing>.98f;
                            int dodge=0;
                            if(Dodge(ship,target,&dodge)) {
                                input.left=dodge&leftButtonIGC;input.right=dodge&rightButtonIGC;
                                input.up=dodge&upButtonIGC;input.down=dodge&downButtonIGC;
                                input.forward=dodge&forwardButtonIGC;input.backward=dodge&backwardButtonIGC;
                            }
                        }
                    }
                    game.step(input,.02);audio.tick(.02);
                    if(servicing && !game.state().complete && !game.state().cinematic && !game.state().docked && !ship->GetAutopilot())game.returnToBase();
                    if(Training::IsTraining() && Training::GetKillCount()!=lastKills){lastKills=Training::GetKillCount();printf("Arena %d input kills: %d at %.1fs\n",id,lastKills,frame*.02);}
                    for(auto& message:game.state().messages)congratulated|=message.text.find("defeat all waves")!=std::string::npos;
                }
                if(!game.state().complete)fprintf(stderr,"Arena %d input stuck: kills=%d target=%s distance=%.0f ammo=%d hull=%.2f deaths=%d sector=%s\n",id,lastKills,game.state().target.c_str(),game.state().targetDistance,game.state().ammo,game.state().hull,game.state().deaths,game.state().sector.c_str());
                if(lastKills!=49)fprintf(stderr,"Arena %d final: kills=%d hull=%.3f shield=%.3f ammo=%d result=%s\n",id,lastKills,game.state().hull,game.state().shield,game.state().ammo,game.state().result.c_str());
                if(lastKills!=49)for(auto& message:game.state().messages)fprintf(stderr,"Arena %d outcome: %s\n",id,message.text.c_str());
                require(game.state().complete && lastKills==49 && congratulated,"original arena must reach victory through real pilot inputs and weapon kills");
                printf("Original mission %d: input-driven 49-kill victory across %zu hull variants passed.\n",id,hulls.size());
            }
            if((id==6 || id==10) && arenaRuns[id]==0 && !game.state().complete) {
                const auto before=game.state().time;
                const Vector at=ship->GetPosition();
                ship->ReceiveDamage(c_dmgidCollision,1e9f,game.core()->GetLastUpdate(),at,at+Vector(1,0,0),nullptr);
                game.step({},.01);audio.tick(.01);game.camera({});
                require(!game.state().complete && !ship->GetCluster() && game.state().cinematic,"arena death suspends the ship and starts the original death camera before ending");
                for(int frame=0;frame<140 && !game.state().complete;++frame){game.step({},.05);audio.tick(.05);game.camera({});}
                require(game.state().complete && game.state().time-before>=Allegiance::OverrideCameraDuration(true) &&
                    game.state().trainingPostSlideshow && game.state().result=="Training ended: ship destroyed" && !Training::IsTraining(),
                    "arena death ends after the original camera duration and retains the debrief without claiming success");
            }
            if((id==6 || id==10) && arenaRuns[id]++>=1) {
                // Feed real damage/death events through the native site. This audits
                // every authored respawn/wave action separately from flight skill.
                std::set<HullID> hulls;bool congratulated=false;int kills=0;
                for(int frame=0;frame<40000 && !game.state().complete;++frame) {
                    if(Training::GetKillCount()<49) {
                        std::vector<TRef<IshipIGC>> targets;
                        for(auto* l=game.core()->GetShips()->first();l;l=l->next()) {
                            auto* enemy=l->data();if(enemy->GetCluster() && enemy->GetSide()!=ship->GetSide() && enemy->GetPilotType()==c_ptWingman)targets.emplace_back(enemy);
                        }
                        for(auto& enemy:targets)if(Training::GetKillCount()<49) {
                            hulls.insert(enemy->GetBaseHullType()->GetObjectID());
                            Vector hit=enemy->GetPosition();enemy->ReceiveDamage(c_dmgidCollision,1e9f,game.core()->GetLastUpdate(),hit,hit+Vector(1,0,0),ship);
                        }
                    }
                    kills=Training::GetKillCount();game.step({},.05);audio.tick(.05);
                    for(auto& message:game.state().messages)congratulated|=message.text.find("defeat all waves")!=std::string::npos;
                }
                if(!(game.state().complete && game.state().trainingPostSlideshow && game.state().result=="Training ended" && kills==49 && hulls.size()>=7 && congratulated)) {
                    fprintf(stderr,"Arena %d debrief: complete=%d slideshow=%d kills=%d hulls=%zu congratulated=%d result=%s hull=%.3f time=%.1f\n",id,game.state().complete,game.state().trainingPostSlideshow,kills,hulls.size(),congratulated,game.state().result.c_str(),game.state().hull,game.state().time);
                    for(const auto& message:game.state().messages)fprintf(stderr,"Arena %d message: %s\n",id,message.text.c_str());
                }
                require(game.state().complete && game.state().trainingPostSlideshow && game.state().result=="Training ended" && kills==49 && hulls.size()>=7 && congratulated,"all 49 arena/free-flight kills, hull changes, respawns and victory cue must finish with the original debrief");
                printf("Original mission %d: all waves, 49 damage-event kills, %zu enemy hull variants and victory narration passed.\n",id,hulls.size());
            }
            if(id==2) {
                bool fullThrottle=false,boosted=false,homeward=false;
                for(int frame=0;frame<36000 && !game.state().complete;++frame) {
                    Native::FlightInput input;
                    if(!ship->GetAutopilot()) {
                        input.throttle=(frame/100)%2?1.f:0.f;
                        input.boost=(frame%200)<20;
                        if(frame%20==0)game.recordKey(TK_FireBooster);
                    } else homeward=true;
                    game.step(input,.05);audio.tick(.05);
                    fullThrottle|=ship->GetControls().jsValues[c_axisThrottle]>.9f;
                    boosted|=(ship->GetStateM()&afterburnerButtonIGC)!=0;
                }
                require(game.state().complete && fullThrottle && boosted && homeward,"flight lesson must finish through authored controls, autopilot and actual base collision");
                printf("Original flight lesson completed through narration, controls and return to base.\n");
            }
            if(id==5) {
                auto findShip=[&](std::string_view name)->IshipIGC*{for(auto* l=game.core()->GetShips()->first();l;l=l->next())if(name==l->data()->GetName())return l->data();return nullptr;};
                bool towers=false,mining=false,retreated=false,attacked=false,found=false;
                std::string previous;
                for(int frame=0;frame<36000 && !game.state().complete;++frame) {
                    if(frame%20==0)game.recordKey(TK_ThrustForward);
                    std::string text=game.state().messages.empty()?"":game.state().messages.back().text;
                    if(text!=previous){printf("Commander lesson: %s\n",text.c_str());previous=text;}
                    if(text.find("Tower Layer to select")!=std::string::npos) {
                        auto* layer=findShip("Tower Layer 01");require(layer && game.pickCommandUnit(layer->GetObjectID()),"select original tower layer");
                    }
                    if(!towers && text.find("Right click in space")!=std::string::npos) {
                        auto p=game.core()->GetWarp(1050)->GetPosition();int i=0;
                        for(auto name:{"Tower Layer 01","Tower Layer 02","Tower Layer 03"}) {
                            auto* layer=findShip(name);require(layer && game.pickCommandUnit(layer->GetObjectID()),"pick tower for deployment");game.commandOrder(c_cidBuild);
                            require(game.commandPosition({p.x+float(i++*200),p.y-500,p.z}),"issue tower build order at a waypoint");
                        }towers=true;
                    }
                    if(text.find("Sector Map to change view")!=std::string::npos)require(game.viewSector(1051),"view Mars through sector map");
                    if(text.find("Miner to select")!=std::string::npos){auto* miner=findShip("Miner 02");require(miner && game.pickCommandUnit(miner->GetObjectID()),"select original miner");}
                    if(!mining && text.find("He3 asteroid to begin mining")!=std::string::npos) {
                        int i=0;for(auto name:{"Miner 02","Miner 03","Miner 04"}) {
                            auto* miner=findShip(name);require(miner,"original miner exists");auto targets=game.orderTargets(miner->GetObjectID(),c_cidMine);
                            auto it=std::find_if(targets.begin(),targets.end(),[&](auto& t){return t.sectorId==1051;});require(it!=targets.end(),"known helium asteroid in Mars");
                            require(game.order(miner->GetObjectID(),it->type,it->id,c_cidMine),"original miner accepts mining order");++i;
                        }mining=true;
                    }
                    if(!retreated && text.find("back to base for safety")!=std::string::npos) {
                        for(auto name:{"Miner 02","Miner 03","Miner 04"}){auto* miner=findShip(name);require(miner && game.order(miner->GetObjectID(),OT_station,1052,c_cidGoto),"miner returns to base");}retreated=true;
                    }
                    if(!attacked && text.find("stealth fighter to attack")!=std::string::npos) {
                        IshipIGC* fighter=nullptr;IshipIGC* enemy=nullptr;
                        for(auto* l=game.core()->GetShips()->first();l;l=l->next()){auto* s=l->data();if(s->GetCluster() && s->GetSide()==ship->GetSide() && s->GetPilotType()==c_ptWingman && s!=ship && s->GetCluster()->GetObjectID()==1051)fighter=s;else if(s->GetSide()!=ship->GetSide())enemy=s;}
                        require(fighter && enemy && game.order(fighter->GetObjectID(),OT_ship,enemy->GetObjectID(),c_cidAttack),"fighter accepts combat order");attacked=true;
                    }
                    if(!found && text.find("Take Me To")!=std::string::npos){auto* miner=findShip("Miner 01");require(miner && game.takeMeTo(miner->GetObjectID()),"team display locates original miner");found=true;}
                    game.step({},.05);audio.tick(.05);game.camera({});
                }
                require(game.state().complete && towers && mining && retreated && attacked && found,"original commander tutorial must finish through native orders and actual combat");
            }
            if(id==3) {
                auto* cluster=ship->GetCluster();auto view=Native::trainingHost->view;auto camera=Native::trainingHost->camera;
                DataShipIGC retiredData{};retiredData.shipID=game.core()->GenerateNewShipID();
                retiredData.hullID=retiredData.sideID=retiredData.baseObjectID=NA;
                retiredData.pilotType=c_ptWingman;retiredData.partsOffset=sizeof(retiredData);
                TRef<IshipIGC> retired=static_cast<IshipIGC*>(game.core()->CreateObject(game.core()->GetLastUpdate(),OT_ship,&retiredData,sizeof(retiredData)));
                require(retired!=nullptr,"create retained projectile-credit fixture");retired->Release();retired->Terminate();
                Training::ShipDied(retired);require(!ship->GetCluster(),"training death must suspend the pilot ship even after its attacker terminated");
                for(int i=0;i<120;++i){game.step({},.05);audio.tick(.05);game.camera({});}
                require(ship->GetCluster()==cluster && ship->GetFraction()>0 && Native::trainingHost->view==view && Native::trainingHost->camera==camera,"training death restores same ship, sector and camera");
                std::string lastPrompt;int firingTarget=-1,burst=0;bool missiles=false,landing=false,approached=false;int destroyed=0;
                for(int frame=0;frame<150000 && !game.state().complete;++frame) {
                    Native::FlightInput input;auto& state=game.state();
                    if(frame%50==0)game.recordKey(TK_FireBooster);
                    std::string prompt;
                    for(auto i=state.messages.rbegin();i!=state.messages.rend();++i)if(i->text.starts_with("Commander (")){prompt=i->text;break;}
                    if(prompt!=lastPrompt) {
                        printf("Weapons lesson: %s\n",prompt.c_str());lastPrompt=prompt;
                        if(prompt.find("center of your display")!=std::string::npos)game.recordKey(TK_TargetCenter);
                        if(prompt.find("Target an enemy")!=std::string::npos){if(game.recordKey(TK_TargetEnemy))game.targetKey(TK_TargetEnemy);}
                        if(prompt.find("Left click to fire")!=std::string::npos)burst=1;
                        if(prompt.find("secondary weapon")!=std::string::npos){missiles=true;burst=1;}
                        if(prompt.find("Destroy current target")!=std::string::npos)firingTarget=ship->GetCommandTarget(c_cmdCurrent)?ship->GetCommandTarget(c_cmdCurrent)->GetObjectID():-1;
                        if(prompt.find("target the outpost")!=std::string::npos){game.selectTarget(OT_station,1010);}
                        if(prompt.find("Land in a green hangar")!=std::string::npos)landing=true;
                    }
                    if(firingTarget>=0 && !game.core()->GetShip(ShipID(firingTarget))){firingTarget=-1;++destroyed;}
                    glm::vec3 toward=state.targetPosition;
                    if(landing) {
                        auto* base=game.core()->GetStation(1010);auto* type=base->GetStationType();
                        Vector p=type->GetLandPosition(0,0),direction=type->GetLandDirection(0,0);
                        Vector approach=base->GetPosition()+(p+direction*450)*base->GetOrientation();
                        if((ship->GetPosition()-approach).Length()<5)approached=true;
                        Vector destination=approached?base->GetPosition()+(p+direction*10)*base->GetOrientation():approach;
                        toward={destination.x,destination.y,destination.z};
                    }
                    if(state.targetId>=0 || landing) {
                        auto direction=glm::normalize(toward-state.position);float facing=glm::dot(direction,state.forward);
                        input.yaw=std::clamp(-glm::dot(direction,state.right)*3,-1.f,1.f);input.pitch=std::clamp(-glm::dot(direction,state.up)*3,-1.f,1.f);
                        if(landing)input.throttle=facing>.985f?(approached?.08f:std::clamp(glm::distance(toward,state.position)/1000,.03f,.4f)):0;
                        if(firingTarget>=0 || burst>0){input.fire=!missiles && facing>.99f;input.missile=missiles && facing>.99f;if(input.fire || input.missile)--burst;}
                    }
                    game.step(input,.02);audio.tick(.02);
                }

                require(game.state().complete && landing && destroyed>=3,"weapons lesson must finish through real targeting, weapon kills and manual docking");

            }
            if(id==8) {
                bool successful=repairRuns++>0;
                bool mounted=false,repaired=false,defending=false,escorting=false,crossed=false;int repairTarget=-1;std::string previous;
                auto findShip=[&](std::string_view name)->IshipIGC*{for(auto* l=game.core()->GetShips()->first();l;l=l->next())if(name==l->data()->GetName() && l->data()->GetCluster())return l->data();return nullptr;};
                for(int frame=0;frame<150000 && !game.state().complete;++frame) {
                    auto& state=game.state();Native::FlightInput input;
                    std::string prompt;
                    for(auto i=state.messages.rbegin();i!=state.messages.rend();++i)if(i->text.starts_with("Commander (")){prompt=i->text;break;}
                    if(prompt!=previous) {
                        printf("Repair lesson: %s\n",prompt.c_str());previous=prompt;
                        if(prompt.find("Swap the Nanite")!=std::string::npos){require(game.swapCargo(Native::PartKind::Weapon,0),"mount authored nanite reserve");mounted=true;}
                        if(prompt.find("Repair the station")!=std::string::npos){if(!ship->GetAutopilot())game.toggleAutopilot();}
                        if(prompt.find("Nice work")!=std::string::npos){repaired=true;if(ship->GetAutopilot())game.toggleAutopilot();}
                        if(prompt.find("Defend miners")!=std::string::npos){auto* miner=findShip("Miner 01");require(miner,"miner for repair exercise");repairTarget=miner->GetObjectID();defending=true;}
                        if(prompt.find("Meet me in front")!=std::string::npos){repairTarget=-1;if(ship->GetCommandID(c_cmdQueued)!=c_cidNone)require(game.acceptCommand(true),"repair escort waypoint acceptance");escorting=true;}
                        if(prompt.find("Go Go Go")!=std::string::npos || prompt.find("Defend bombers")!=std::string::npos){auto* bomber=findShip("Commander");require(bomber,"bomber escort exists");repairTarget=bomber->GetObjectID();}
                    }
                    if(repaired && !escorting && ship->GetCommandID(c_cmdQueued)==c_cidGoto) {
                        require(game.acceptCommand(true),"repair escort waypoint acceptance");repairTarget=-1;escorting=true;
                    }
                    if(repaired && !escorting && repairTarget<0)if(auto* miner=findShip("Miner 01"))repairTarget=miner->GetObjectID();
                    crossed|=ship->GetCluster() && ship->GetCluster()->GetObjectID()==1052;
                    if(escorting && repairTarget<0)input.active=false;
                    if(auto* repair=repairTarget<0?nullptr:game.core()->GetShip(ShipID(repairTarget))) {
                        if(repair->GetCluster()!=ship->GetCluster()) {
                            if(ship->GetCommandTarget(c_cmdCurrent)!=repair)game.selectTarget(OT_ship,repairTarget);
                            if(!ship->GetAutopilot())game.toggleAutopilot();input.active=false;
                        } else if(successful && escorting) {
                            if(ship->GetAutopilot())game.toggleAutopilot();
                            if(state.targetId!=repairTarget)game.selectTarget(OT_ship,repairTarget);
                            auto* nan=static_cast<IweaponIGC*>(ship->GetMountedPart(ET_Weapon,0));require(nan && nan->GetProjectileType()->GetPower()<0,"escort retains mounted nanite weapon");
                            Vector direction=(repair->GetPosition()-ship->GetPosition()).Normalize();float time=solveForLead(ship,repair,nan,&direction,1);
                            ControlData controls;turnToFace(direction,.02f,ship,&controls,1);
                            input.yaw=controls.jsValues[c_axisYaw];input.pitch=controls.jsValues[c_axisPitch];input.roll=controls.jsValues[c_axisRoll];
                            float distance=(repair->GetPosition()-ship->GetPosition()).Length();
                            float standoff=ship->GetRadius()+repair->GetRadius()+80;
                            float speed=repair->GetVelocity()*ship->GetOrientation().GetForward()+(distance-standoff)*.3f;
                            input.throttle=std::clamp(speed/ship->GetHullType()->GetMaxSpeed(),0.f,1.f);
                            input.backward=distance<standoff-20;input.boost=distance>1000 && direction*ship->GetOrientation().GetForward()>.98f;
                            input.fire=repair->GetFraction()<.99f && time<=nan->GetLifespan()*.95f && direction*ship->GetOrientation().GetForward()>.995f;

                        } else {
                            if(state.targetId!=repairTarget)game.selectTarget(OT_ship,repairTarget);
                            auto d=state.hasWeaponSolution?state.leadDirection:glm::normalize(state.targetPosition-state.position);float facing=glm::dot(d,state.forward);
                            input.yaw=std::clamp(-glm::dot(d,state.right)*3,-1.f,1.f);input.pitch=std::clamp(-glm::dot(d,state.up)*3,-1.f,1.f);
                            input.throttle=facing>.9f?std::clamp((state.targetDistance-100)/600,0.f,1.f):0;
                            input.boost=facing>.98f && state.targetDistance>900;
                            input.fire=facing>.98f && state.inRange;
                        }
                    }
                    game.step(input,.02);audio.tick(.02);
                }
                if(!game.state().complete)fprintf(stderr,"Repair stuck: target=%s range=%f hull=%f mounted=%d repaired=%d defend=%d escort=%d crossed=%d autopilot=%d\n",game.state().target.c_str(),game.state().targetDistance,game.state().hull,mounted,repaired,defending,escorting,crossed,ship->GetAutopilot());
                require(game.state().complete && mounted && repaired && defending && escorting && crossed,"nanite lesson completes through reserve mounting, real repairs, miner defense and bomber escort across an aleph");
                require(!successful || !game.core()->GetStation(1051),"repair escort must reach the authored enemy-base victory outcome");
            }
            if(id==4) {
                if(combatRuns++==0) {
                    bool sent=false,cloaked=false,combat=false,landed=false,crossed=false;int accepted=0,escortClearance=0;std::string previous;
                    for(int frame=0;frame<180000 && !game.state().complete;++frame) {
                        Native::FlightInput input;auto& state=game.state();
                        if(frame%50==0)game.recordKey(TK_FireBooster);
                        std::string prompt=state.messages.empty()?"":state.messages.back().text;
                        if(prompt!=previous) {
                            printf("Engagement lesson: %s\n",prompt.c_str());previous=prompt;
                            if(prompt.find("again to send the chat")!=std::string::npos){require(game.sendChat(Native::ChatChannel::Everyone,"Ready"),"lesson all-pilots chat");sent=true;}
                            if(prompt.find("activate your cloak")!=std::string::npos){if(game.recordKey(TK_ToggleCloak))game.toggleCloak();cloaked=true;}
                            if(prompt.find("Target an enemy miner")!=std::string::npos){if(game.recordKey(TK_TargetEnemy))game.targetKey(TK_TargetEnemy);combat=true;}
                            if(prompt.find("accept the command")!=std::string::npos && ship->GetCommandID(c_cmdQueued)!=c_cidNone){require(game.acceptCommand(true),"authored commander order acceptance");++accepted;combat=ship->GetCommandID(c_cmdAccepted)==c_cidAttack;}
                            if(prompt.find("Land in the expansion complex")!=std::string::npos) {
                                for(auto* l=ship->GetSide()->GetStations()->first();l;l=l->next())if(l->data()->GetCluster()==ship->GetCluster()) {
                                    game.selectTarget(OT_station,l->data()->GetObjectID());require(game.state().targetId==l->data()->GetObjectID(),"target newly constructed base");game.toggleAutopilot();landed=true;break;
                                }
                            }
                        }
                        crossed|=ship->GetCluster() && ship->GetCluster()->GetObjectID()==1031;
                        if(combat && state.targetEnemy && state.targetId>=0) {
                            auto direction=state.hasWeaponSolution?state.leadDirection:glm::normalize(state.targetPosition-state.position);
                            float facing=glm::dot(direction,state.forward);
                            input.yaw=std::clamp(-glm::dot(direction,state.right)*3,-1.f,1.f);input.pitch=std::clamp(-glm::dot(direction,state.up)*3,-1.f,1.f);
                            input.fire=state.targetDistance<750 && facing>.99f;
                            input.throttle=facing>.8f?std::clamp((state.targetDistance-180)/900,0.f,1.f):0;
                            input.boost=state.targetDistance>1200 && facing>.97f;
                        }
                        if(accepted>=5 && !landed && escortClearance++<500)input.up=true;
                        game.step(input,.02);audio.tick(.02);
                    }
                    if(!game.state().complete)for(auto* l=game.core()->GetShips()->first();l;l=l->next()){auto* s=l->data();auto* t=s->GetCommandTarget(c_cmdPlan);fprintf(stderr,"Ship %s sector=%d state=%x accepted=%d current=%d plan=%d target=%s autopilot=%d distance=%f\n",s->GetName(),s->GetCluster()?s->GetCluster()->GetObjectID():-1,s->GetStateM(),s->GetCommandID(c_cmdAccepted),s->GetCommandID(c_cmdCurrent),s->GetCommandID(c_cmdPlan),t?t->GetName():"none",s->GetAutopilot(),t?(s->GetPosition()-t->GetPosition()).Length():0);fprintf(stderr,"  radius=%f targetRadius=%f throttle=%f pitch=%f yaw=%f speed=%f facing=%f\n",s->GetRadius(),t?t->GetRadius():0,s->GetControls().jsValues[3],s->GetControls().jsValues[1],s->GetControls().jsValues[0],s->GetVelocity().Length(),t?(t->GetPosition()-s->GetPosition()).Normalize()*s->GetOrientation().GetForward():0);}
                    if(!game.state().complete)fprintf(stderr,"Engagement stuck: sector=%s target=%s range=%f hull=%f accepted=%d combat=%d autopilot=%d\n",game.state().sector.c_str(),game.state().target.c_str(),game.state().targetDistance,game.state().hull,accepted,combat,ship->GetAutopilot());
                    require(game.state().complete && sent && cloaked && crossed && landed && accepted>=4,"engagement lesson must finish through chat, orders, aleph transit, cloak, combat, construction and docking");
                } else {
                Training::GetChatCondition chat(CHAT_EVERYONE);chat.Start();
                require(!chat.Evaluate() && game.sendChat(Native::ChatChannel::Everyone,"Training communication check") && chat.Evaluate(),"native outgoing chat must notify the original lesson recipient condition");
                auto history=game.state().messages.size();
                require(game.sendChat(Native::ChatChannel::Target,"Default individual self chat",NA) && game.state().messages.size()==history+1 && game.state().messages.back().chatTarget==CHAT_INDIVIDUAL,"BaseClient's default individual recipient is the local ship");
                require(!game.sendChat(Native::ChatChannel::Target,"Invalid recipient",SHRT_MAX),"invalid individual chat recipient");
                auto* condition=new Training::GetKeyCondition(TK_ThrustForward);condition->Start();Training::g_pMission->AddWaitCondition(condition);
                TRef<IstationIGC> last;
                for(auto* l=ship->GetSide()->GetStations()->first();l;l=l->next())if(l->data()->GetBaseStationType()->HasCapability(c_sabmRestart))last=l->data();
                require(last,"training needs a restart station for the loss test");
                for(auto* l=ship->GetSide()->GetStations()->first();l;){auto* station=l->data();l=l->next();if(station!=last && station->GetBaseStationType()->HasCapability(c_sabmRestart))station->Terminate();}
                Training::KillStationEvent(last,nullptr);
                for(int i=0;i<100 && !game.state().complete;++i){game.step({},.05);audio.tick(.05);game.recordKey(TK_ThrustForward);}
                require(game.state().complete,"station loss replaces active waits and ends the original mission");
                }
            }
            Training::SetSkipPostSlideshow();Training::EndMission();Training::EndMission();game.step({},.01);
            require(!Training::IsTraining() && game.state().complete,"authored deferred termination completes native session");
            printf("Original training %d: world, clock, controls, narration, camera and teardown passed\n",id);
        }
        require(!Native::trainingHost && !Training::IsTraining(),"training service ownership survives repeated missions");
        {
            audio.stopAll();Native::SessionOptions options;options.trainingMission=2;
            Native::Game game(assets.root(),[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
            game.startTraining(audio);Training::SetSkipPostSlideshow();Training::EndMission();game.step({},.01);
            require(game.state().complete && !game.state().trainingPostSlideshow && game.state().result=="Training ended" && !Training::IsTraining(),
                "explicit training exit suppresses the debrief independently of result text");
        }
        require(!Native::trainingHost && !Training::IsTraining(),"explicit training exit releases service ownership");return 0;
    }catch(const std::exception& e){fprintf(stderr,"training: %s\n",e.what());return 1;}
}
