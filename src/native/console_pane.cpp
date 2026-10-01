#include "console_pane.hpp"
#include "../ClientCore/AuthoredText.h"
#include <algorithm>
namespace Native {
namespace {
constexpr const char* roots[]={"LastOrderImage","CriticalTipImageTranslated","VoteTextImageTranslated","TipTextImage","TipTextImage1","TipTextImage2"};
std::string_view textName(HudString binding) {
    switch(binding){
    case HudString::QueuedOrder:return "QueuedOrder";case HudString::AcceptedOrder:return "AcceptedOrder";
    case HudString::Critical0:return "CriticalTipText0";case HudString::Critical1:return "CriticalTipText1";
    case HudString::Critical2:return "CriticalTipText2";case HudString::Critical3:return "CriticalTipText3";
    case HudString::Tip0:return "TipText";case HudString::Tip1:return "TipText1";case HudString::Tip2:return "TipText2";
    case HudString::Vote:return "CurrentVote";default:return {};
    }
}
}
ConsolePane::ConsolePane(Assets& assets) {
    OriginalUi ui(assets);overlays_=std::make_unique<UiOverlayLayout>(ui,std::initializer_list<std::string_view>{roots[0],roots[1],roots[2],roots[3],roots[4],roots[5]});
    programs_.reserve(std::size(roots));
    for(size_t i=0;i<std::size(roots);++i){programs_.emplace_back(ui,roots[i],0);paintOrder_[i]=i;}
    std::sort(paintOrder_.begin(),paintOrder_.end(),[&](size_t a,size_t b){return overlays_->index(a)>overlays_->index(b);});
    for(const auto& draw:programs_[1].evaluate({}))if(draw.textBinding==HudString::Critical0){
        criticalWidth_=draw.bounds.z;criticalFont_=assets.font(programs_[1].fonts()[draw.font].symbol,programs_[1].fonts()[draw.font].height,programs_[1].fonts()[draw.font].bold);break;
    }
    if(!criticalFont_)throw std::runtime_error("Original critical tip font binding missing");
}
void ConsolePane::reset(){tips_={};lastMessage_=displayRevision_=0;invalid_=true;}
bool ConsolePane::consume(const GameState& game) {
    auto last=game.messages.empty()?0:game.messages.back().serial;
    if(!last)return false; // Clearing chat history does not clear console tips.
    if(last<lastMessage_)reset();
    bool changed=false;
    if(last!=lastMessage_){
        auto begin=std::upper_bound(game.messages.begin(),game.messages.end(),lastMessage_,[](uint64_t serial,const auto& message){return serial<message.serial;});
        for(auto i=begin;i!=game.messages.end();++i)if(i->tip){
            const auto& text=i->displayRevision?i->displayText:i->text;bool overflow=Allegiance::FitText(text,int(criticalWidth_),true,[&](unsigned char ch){return criticalFont_->glyphs[ch].z;})<text.size();
            tips_.post({text,i->displayTime?i->displayTime:i->time,i->serial},i->warning,game.trainingMission!=0,overflow);changed=true;
        }lastMessage_=last;
    }
    auto revision=game.messages.empty()?0:game.messages.back().displayRevision;
    if(revision!=displayRevision_){
        auto update=[&](auto& layer){for(auto& tip:layer)if(tip.serial){
            auto i=std::lower_bound(game.messages.begin(),game.messages.end(),tip.serial,[](const auto& message,uint64_t serial){return message.serial<serial;});
            if(i!=game.messages.end() && i->serial==tip.serial){tip.text=i->displayRevision?i->displayText:i->text;changed=true;}
        }};update(tips_.ordinary);update(tips_.critical);displayRevision_=revision;
    }return changed;
}
const MenuView& ConsolePane::view(glm::ivec2 size,int scale,const GameState& game,int chatLines,bool loadout,bool shown,double now) {
    bool changed=consume(game);scale=originalInterfaceScale(size,std::max(1,scale));unsigned visible=tips_.visible(game.trainingMission!=0,now);
    auto modes=originalOverlayModes(game,loadout?ofInLoadout:0);
    auto color=game.playerSide>=0 && game.playerSide<int(game.sideColors.size())?game.sideColors[game.playerSide]:glm::vec4(1);
    if(invalid_ || changed || overlayModes_!=modes || size_!=size || scale_!=scale || chatLines_!=chatLines || loadout_!=loadout || shown_!=shown || visible_!=visible || color_!=color ||
        commandText_[0]!=game.commands[0].text || commandText_[1]!=game.commands[1].text || voteText_!=game.ballot.text){
        overlayModes_=modes;size_=size;scale_=scale;chatLines_=chatLines;loadout_=loadout;shown_=shown;visible_=visible;color_=color;
        commandText_[0]=game.commands[0].text;commandText_[1]=game.commands[1].text;
        voteText_=game.ballot.text;
        rebuild(game,chatLines,loadout,shown,visible);invalid_=false;
    }return view_;
}
void ConsolePane::rebuild(const GameState&,int chatLines,bool loadout,bool shown,unsigned visible) {
    view_={};view_.original=true;view_.overlay=true;view_.classicScale=scale_;view_.canvas={size_.x/scale_,size_.y/scale_};
    if(!shown)return;
    HudValues values;values[HudNumber::ChatLines]=loadout?std::min(chatLines,6):chatLines;values[HudColor::Side]=color_;
    values[HudString::QueuedOrder]=commandText_[0];values[HudString::AcceptedOrder]=commandText_[1];values[HudString::Vote]=voteText_;
    for(int i=0;i<4;++i)if(visible&(1u<<i))values[HudString(size_t(HudString::Critical0)+i)]=tips_.critical[i].text;
    for(int i=0;i<3;++i)values[HudString(size_t(HudString::Tip0)+i)]=tips_.ordinary[i].text;
    for(auto binding:paintOrder_){
        const auto* overlay=overlays_->get(binding,values,glm::vec2(view_.canvas));if(!overlay || !(overlay->mask&overlayModes_))continue;
        auto& program=programs_[binding];glm::vec2 origin=glm::floor(overlay->on);
        for(const auto& draw:program.evaluate(values))if(auto control=authoredHudControl(program,draw,textName(draw.textBinding),origin))view_.artwork.push_back(std::move(*control));
    }
}
}
