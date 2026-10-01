// WinTrek::OnTrekKeyFilter. Popup ownership precedes this filter; ordinary
// screen controls and text composition follow it.
#pragma once
#include "../Igc/TrekKeys.h"
namespace Allegiance {
struct GlobalKeyContext {
    bool splash{},inGame{},overrideView{},lockedDown{},composing{},mission{},quickChatScreen{};
};
enum class GlobalKeyDisposition {Pass,Dispatch,Consume};
constexpr GlobalKeyDisposition FilterGlobalKey(TrekKey key,const GlobalKeyContext& context){
    using D=GlobalKeyDisposition;
    if(context.splash)return D::Pass;
    switch(key){
    case TK_ScrnShot:case TK_Help:case TK_ChatPageUp:case TK_ChatPageDown:return D::Dispatch;
    case TK_ToggleMouse:return context.inGame && context.overrideView && !context.lockedDown && !context.composing?D::Dispatch:D::Pass;
    case TK_MainMenu:return (!context.inGame || !context.overrideView) && !context.lockedDown && !context.composing?D::Dispatch:D::Pass;
    case TK_QuickChatMenu:return context.mission && (!context.inGame || !context.overrideView) && !context.lockedDown && context.quickChatScreen?D::Dispatch:D::Consume;
    default:return D::Pass;
    }
}
}
