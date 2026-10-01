#pragma once
#include "frontend.hpp"
#include "quick_chat.hpp"
#include "../ClientCore/ChatComposition.h"

namespace Native {
// ConsoleData's composition state, independent of the current hangar/flight
// pane. The UI retains IDs, never IGC objects across simulation updates.
class ChatConsole {
public:
    ChatConsole(Assets& assets,const QuickChatCatalog& quick);
    void reset();
    void cancel(Game& game){quit();update(game);}
    void viewChanged(Game& game){if(mode_==Allegiance::ComposeState::Mouse)quit();popupClose();update(game);}
    void startChat(Game& game);
    void startQuick(Game& game);
    void verb(Game& game,int command);
    void recipient(Game& game,int button);
    void wings(Game& game);
    void selectUnits(Game& game);
    bool selectPeer(Game& game,int id);
    void pickCluster(Game& game,int sector);
    bool pickTarget(Game& game,int type,int id);
    bool pickPosition(Game& game,int sector,glm::vec3 position);
    bool key(Game& game,SDL_Keycode key,bool control=false);
    void text(Game& game,std::string_view text);
    bool pointer(Game& game,glm::ivec2 size,glm::vec2 pixel,bool down);
    void hover(glm::ivec2 size,glm::vec2 pixel);
    void sync(Game& game);
    const MenuView& view(glm::ivec2 size,int scale);
    bool active()const{return mode_!=Allegiance::ComposeState::Idle;}
    bool keyboard()const{return mode_>Allegiance::ComposeState::Mouse || !popups_.empty();}
    bool wantsText()const{return mode_>=Allegiance::ComposeState::Chat && mode_<=Allegiance::ComposeState::Shell && popups_.empty();}
    Allegiance::ComposeState mode()const{return mode_;}
    const ChatAudience& audience()const{return audiences_[subject_];}
    const std::string& typed()const{return typed_;}
    const std::optional<OrderTarget>& target()const{return target_;}
    int recipientButton()const;
    const std::array<bool,9>& allowedVerbs()const{return verbs_;}
    int currentVerb()const{return verb_;}
    std::span<const int> subjects()const{return subjects_;}
    std::span<const SoundID> sounds()const{return {sounds_.data(),soundCount_};}
    void clearSounds(){soundCount_=0;}
private:
    void start(Game& game,Allegiance::ComposeState mode,int subject);
    void quit();
    void update(Game& game,bool match=false);
    void setAudience(Game& game,ChatAudience audience);
    void cycle(Game& game);
    void tab(Game& game);
    void send(Game& game);
    bool sendTarget(Game& game,int type,int id);
    bool sendPosition(Game& game,int sector,glm::vec3 position,bool cluster);
    template<class F> bool dispatch(Game& game,F&& action) {
        auto recipients=game.chatSubjects(audience());if(audience().none)return false;
        auto& a=audiences_[subject_];
        if(a.channel==ChatChannel::Target && recipients.empty())a={};
        else if(a.channel==ChatChannel::Group){a.group=recipients;
            if(recipients.size()==1){a.channel=ChatChannel::Target;a.recipient=recipients[0];a.group.clear();}
            else if(recipients.empty())a={};
        }
        if(audience().channel!=ChatChannel::Group)return action(audience().channel,audience().recipient);
        bool sent=false;for(size_t i=0;i<recipients.size();++i)
            sent=action(i?ChatChannel::GroupNoEcho:ChatChannel::Group,recipients[i]) || sent;
        return sent;
    }
    void sound(SoundID id){if(soundCount_<sounds_.size())sounds_[soundCount_++]=id;}
    struct Popup {int menu{-1},selected{};glm::vec4 bounds{};};
    void popupSelect(Game& game,int depth,int row);
    void rebuild();
    void popupLayout();
    void popupClose();
    Assets& assets_;OriginalUi original_;UiOverlayLayout overlays_;const QuickChatCatalog& quick_;
    glm::vec4 wingBounds_{};float commandHeight_{};uint32_t overlayModes_{};
    Allegiance::ComposeState mode_{Allegiance::ComposeState::Idle};
    std::array<ChatAudience,3> audiences_;int subject_{1},verb_{};
    std::string typed_,label_,display_;std::optional<OrderTarget> target_;
    std::vector<Popup> popups_;
    std::array<bool,9> verbs_{};std::vector<int> subjects_;
    MenuView view_;std::vector<OriginalControl> composer_;
    glm::ivec2 size_{};int scale_{1},ship_{-1};glm::vec4 color_{1};
    bool dirty_{true},contentDirty_{true},closing_{},commandView_{};
    Allegiance::ComposeState compiledMode_{Allegiance::ComposeState::Idle};int compiledVerb_{};
    double entered_{},refreshAt_{};
    std::array<SoundID,16> sounds_{};size_t soundCount_{};
};
}
