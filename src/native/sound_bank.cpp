#include "sound_bank.hpp"
#include <climits>
#include "mdl_text.hpp"
#include "sound_gain.hpp"
#include <charconv>
#include <fstream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

namespace Native {
using namespace SoundEngine;
namespace {
using Mdl::Expr;using Mdl::Parser;
const char* soundNames[]={
#define DEFSOUND(id) #id "Sound",
#define DEFEMPTYSOUND(id) #id "Sound",
#include "../Igc/sounds.h"
#undef DEFSOUND
#undef DEFEMPTYSOUND
};
}
struct SoundBank::Impl {
    Assets& assets;WaveLoader loader;
    std::unordered_map<std::string,Expr> expressions;
    std::unordered_map<std::string,TRef<ISoundTemplate>> templates;
    std::unordered_map<std::string,TRef<ISoundMutex>> mutexes;
    std::unordered_map<std::string,double> numbers;
    std::unordered_map<int,std::string> names;
    std::unordered_set<std::string> modules,resolving;
    std::shared_ptr<CategoryGain> effectsGain=std::make_shared<CategoryGain>(),voiceGain=std::make_shared<CategoryGain>();
    Impl(Assets& a,WaveLoader wave):assets(a),loader(std::move(wave)) {
        numbers={{"SFXGain",0},{"VoiceOverGain",0},{"true",1},{"false",0}};
        for(size_t i=0;i<std::size(soundNames);++i){names[int(i)+1001]=soundNames[i];numbers[std::string(soundNames[i])+"Id"]=int(i)+1001;}
        for(const char* name:{"MutexSal","MutexVO"}){TRef<ISoundMutex> mutex;check(CreateSoundMutex(mutex));mutexes[name]=mutex;}
        module("sounddef");
        auto table=expressions.find("soundList");if(table!=expressions.end())for(auto& row:table->second.args) {
            if(row.kind!=Expr::List || row.args.size()!=2 || row.args[1].kind!=Expr::Symbol)throw std::runtime_error("Invalid sound ID table");
            names[int(number(row.args[0]))]=row.args[1].text;
        }
    }
    static void check(HRESULT result){if(FAILED(result))throw std::runtime_error("Invalid original sound-template parameters: "+std::to_string(result));}
    void module(const std::string& name) {
        if(name=="gamepanes" || !modules.insert(name).second)return;
        std::ifstream file(assets.find(name+".mdl"));std::string text{std::istreambuf_iterator<char>(file),{}};Parser parser(text);
        while(!parser.end()) {
            auto symbol=parser.word();
            if(symbol=="use"){auto value=parser.expression();if(value.kind!=Expr::String)throw std::runtime_error("Invalid sound namespace import");parser.need(';');module(value.text);}
            else {parser.need('=');auto value=parser.expression();parser.need(';');expressions.insert_or_assign(std::move(symbol),std::move(value));}
        }
    }
    double number(const Expr& e,unsigned depth=0) {
        if(depth>64)throw std::runtime_error("Cyclic sound numeric constant");
        if(e.kind==Expr::Number)return e.number;
        if(e.kind==Expr::Symbol){auto n=numbers.find(e.text);if(n!=numbers.end())return n->second;
            auto x=expressions.find(e.text);if(x!=expressions.end())return number(x->second,depth+1);}
        throw std::runtime_error("Expected sound number: "+e.text);
    }
    TRef<ISoundTemplate> named(const std::string& name,bool preparation=false) {
        auto key=preparation?"prepare:"+name:name;
        auto found=templates.find(key);if(found!=templates.end())return found->second;
        auto expression=expressions.find(name);if(expression==expressions.end())return nullptr;
        if(!resolving.insert(key).second)throw std::runtime_error("Cyclic sound template: "+name);
        try{auto result=build(expression->second,preparation);resolving.erase(key);templates.emplace(key,result);return result;}
        catch(...){resolving.erase(key);throw;}
    }
    TRef<ISoundTemplate> build(const Expr& e,bool preparation=false) {
        if(e.kind==Expr::Symbol){auto value=named(e.text,preparation);if(value)return value;throw std::runtime_error("Unknown sound: "+e.text);}
        if(e.kind!=Expr::Call)throw std::runtime_error("Expected sound template");
        auto count=[&](size_t n){if(e.args.size()!=n)throw std::runtime_error("Wrong argument count: "+e.text);};
        auto n=[&](size_t i){return float(number(e.args.at(i)));};
        auto sound=[&](size_t i){return build(e.args.at(i),preparation);};
        TRef<ISoundTemplate> out;
        if(e.text=="ImportWave") {
            count(1);if(e.args[0].kind!=Expr::String)throw std::runtime_error("Expected wave filename");
            std::filesystem::path path;try{path=assets.find(e.args[0].text+".ogg");}catch(const std::exception&){path=assets.find(e.args[0].text+".wav");}
            auto pcm=loader(path);check(CreateWaveFileSoundTemplate(out,pcm));
        } else if(e.text=="GainSound"){
            count(2);const auto& gain=e.args[0];
            if(gain.kind==Expr::Symbol && (gain.text=="SFXGain" || gain.text=="VoiceOverGain"))
                out=new CategoryTemplate(gain.text=="SFXGain"?effectsGain:voiceGain,sound(1));
            else check(CreateGainSoundTemplate(out,sound(1),n(0)));
        }
        else if(e.text=="PitchSound"){count(2);check(CreatePitchSoundTemplate(out,sound(1),n(0)));}
        else if(e.text=="PrioritySound"){count(2);check(CreatePrioritySoundTemplate(out,sound(1),n(0)));}
        else if(e.text=="ThreeDSound"){count(2);check(Create3DSoundTemplate(out,sound(1),n(0)));}
        else if(e.text=="ConeSound"){count(4);check(CreateSoundConeTemplate(out,sound(3),n(0),n(1),n(2)));}
        else if(e.text=="LoopingSound"){count(1);check(CreateLoopingSoundTemplate(out,sound(0)));}
        else if(e.text=="ASRSound"){count(3);check(CreateASRSoundTemplate(out,sound(2),n(0),n(1)));}
        else if(e.text=="PairedSound"){count(2);check(CreatePairedSoundTemplate(out,sound(0),sound(1)));}
        else if(e.text=="RepeatingFireSound"){count(2);check(CreateRepeatingFireSoundTemplate(out,sound(1),n(0)));}
        else if(e.text=="IntermittentSound"){count(3);if(preparation)return sound(2);check(CreateIntermittentSoundTemplate(out,sound(2),n(0),bool(n(1))));}
        else if(e.text=="SerializedSound") {
            count(5);if(preparation)return sound(4);auto mutex=mutexes.find(e.args[0].text);if(mutex==mutexes.end())throw std::runtime_error("Unknown sound mutex");
            check(CreateSerializedSoundTemplate(out,sound(4),mutex->second,n(1),n(2),bool(n(3))));
        } else if(e.text=="RandomSound") {
            count(1);if(e.args[0].kind!=Expr::List)throw std::runtime_error("Expected weighted sound list");
            if(preparation) {
                for(auto& item:e.args[0].args){auto next=build(item.args.at(0),true);if(!out)out=next;else {TRef<ISoundTemplate> pair;check(CreatePairedSoundTemplate(pair,out,next));out=pair;}}
                return out;
            }
            TRef<IRandomSoundTemplate> random;check(CreateRandomSoundTemplate(random));
            for(auto& item:e.args[0].args){if(item.kind!=Expr::List || item.args.size()!=2)throw std::runtime_error("Invalid weighted sound");check(random->AddSoundTemplate(build(item.args[0]),float(number(item.args[1]))));}out=random;
        } else throw std::runtime_error("Unsupported sound template: "+e.text);
        return out;
    }
};
SoundBank::SoundBank(Assets& assets,WaveLoader load):impl_(std::make_unique<Impl>(assets,std::move(load))){}
SoundBank::~SoundBank()=default;
TRef<ISoundTemplate> SoundBank::sound(int id){auto it=impl_->names.find(id);return it==impl_->names.end()?nullptr:impl_->named(it->second);}
TRef<ISoundTemplate> SoundBank::sound(const std::string& name){return impl_->named(name);}
std::vector<int> SoundBank::ids()const{std::vector<int> result;for(auto& [id,name]:impl_->names)result.push_back(id);std::sort(result.begin(),result.end());return result;}
TRef<ISoundTemplate> SoundBank::preparation(int id){auto it=impl_->names.find(id);return it==impl_->names.end()?nullptr:impl_->named(it->second,true);}
int SoundBank::registerSound(const std::string& name) {
    if(!impl_->expressions.contains(name))throw std::runtime_error("Unknown voice template: "+name);
    int existing=INT_MAX;for(auto& [id,value]:impl_->names)if(value==name)existing=std::min(existing,id);
    if(existing!=INT_MAX)return existing;
    int id=20000;while(impl_->names.contains(id) && id<32767)++id;
    if(id==32767)throw std::runtime_error("Native sound ID capacity exceeded");
    impl_->names.emplace(id,name);return id;
}
void SoundBank::resetSpeech(){for(auto& [name,mutex]:impl_->mutexes)mutex->Reset();}
void SoundBank::categoryGains(float effects,float voice){impl_->effectsGain->set(effects);impl_->voiceGain->set(voice);}
}
