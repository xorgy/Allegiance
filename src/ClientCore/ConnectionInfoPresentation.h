#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>

namespace Allegiance {
// WinTrek::ShowPlayerPings presentation, independent of the transport/UI engine.
struct PlayerConnectionInfo {
    int32_t pilot{-1};uint16_t ping{};uint8_t loss{},reserved{};
    bool operator==(const PlayerConnectionInfo&)const=default;
};
static_assert(sizeof(PlayerConnectionInfo)==8);
inline const char* PingColor(unsigned ping){return ping<150?"green":ping<300?"yellow":"red";}
inline const char* LossColor(unsigned loss){return loss<5?"green":loss<15?"yellow":"red";}
inline std::string PlayerConnectionReport(std::span<const PlayerConnectionInfo> players,int self){
    std::string own="<Color|yellow><Font|medBoldVerdana>Connection Info of Players<Font|smallFont><Color|white><p><p>",others;
    unsigned sumPing=0,sumLoss=0;
    auto colored=[](const char* color,unsigned value,bool padded,unsigned threshold){return std::string("<Color|")+color+">"+(padded && value<threshold?"0":"")+std::to_string(value);};
    for(const auto& player:players){
        unsigned ping=player.ping>1000?999:std::max(10u,unsigned(player.ping)),loss=player.loss>100?99:player.loss;
        sumPing+=ping;sumLoss+=loss;
        if(player.pilot==self)own+="<Color|cyan>Your Connection Info<Color|white><p>Ping: "+colored(PingColor(ping),ping,false,100)+"ms<Color|white>\tPacket Loss: "+colored(LossColor(loss),loss,false,10)+"%<Color|white><p><p>";
        else others+="Ping: "+colored(PingColor(ping),ping,true,100)+"ms<Color|white>\tPacket Loss: "+colored(LossColor(loss),loss,true,10)+"%<Color|white>\t\tName: <Color|cyan><String|pilot"+std::to_string(player.pilot)+"><Color|white><p>";
    }
    if(!players.empty()){
        unsigned ping=sumPing/players.size(),loss=sumLoss/players.size();
        others+="<p>Average Ping: "+colored(PingColor(ping),ping,false,100)+"ms<Color|white>\tAverage Packet Loss: "+colored(LossColor(loss),loss,false,10)+"%";
    }
    return own+others;
}
}
