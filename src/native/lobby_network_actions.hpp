#pragma once
#include "frontend.hpp"
#include "network_records.hpp"
namespace Native {
// Client action ordering from TeamScreen: Team Ready and JOIN/CANCEL leave
// personal AFK; settings withdraw force-ready before setting personal AFK.
// Details/previous scores cancel a request without simulating a JOIN click.
template<class Emit>
void emitLobbyNetworkActions(const FrontResult& result,bool classic,bool ready,Emit emit) {
    auto action=[&](Net::Operation operation,int first,int second=0,int third=0){
        Net::Action record{};record.op=operation;record.args[0]=first;record.args[1]=second;record.args[2]=third;emit(record);
    };
    switch(result.effect){
    case FrontEffect::ChangeAlliance:{
        Net::Action record{};record.op=Net::Operation::Lobby;record.args[0]=int(LobbyAction::Alliance);
        record.args[1]=result.value;record.args[2]=result.targetId;record.args[3]=result.command;emit(record);break;}
    case FrontEffect::NetworkJoinTeam:
        if(classic && !ready)action(Net::Operation::Ready,1);
        action(Net::Operation::Team,result.value);break;
    case FrontEffect::LobbyAction:
        if(classic && !ready && (result.command==int(LobbyAction::ForceReady) || result.command==int(LobbyAction::CancelRequest)))action(Net::Operation::Ready,1);
        action(Net::Operation::Lobby,result.command,result.value,result.targetId);break;
    case FrontEffect::LobbyVisit:
        if(result.value){action(Net::Operation::Lobby,int(LobbyAction::ForceReady));action(Net::Operation::Ready,0);}
        if(result.command)action(Net::Operation::Lobby,int(LobbyAction::CancelRequest));break;
    default:break;
    }
}
}
