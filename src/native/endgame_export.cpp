#include "game.hpp"
#include "legacy.hpp"
#include "igc.h"
#include <fstream>
#include <iomanip>
#include <locale>
#include <stdexcept>
namespace Native {
namespace {
std::string xmlText(std::string_view value){
    std::string out;out.reserve(value.size());
    for(unsigned char ch:value)switch(ch){
    case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;
    case '\"':out+="&quot;";break;case '\'':out+="&apos;";break;
    default:if(ch>=32 || ch=='\t' || ch=='\n' || ch=='\r')out+=char(ch);break;
    }return out;
}
}
void Game::saveEndgame(const std::filesystem::path& file,const EndgameState& results,std::string_view reason,std::string_view server)const {
    if(!results.frozen)throw std::runtime_error("No completed game statistics");
    std::ofstream out(file,std::ios::binary|std::ios::trunc);out.imbue(std::locale::classic());
    if(!out)throw std::runtime_error("error opening file "+file.string());
    SYSTEMTIME time;GetLocalTime(&time);
    auto field=[&](const char* name,const auto& value,int indent=4){out<<std::string(indent,' ')<<'<'<<name<<'>'<<value<<"</"<<name<<">\n";};
    out<<"<xml>\n<GameInfo>\n  <Date>"<<std::setfill('0')<<std::setw(2)<<time.wMonth<<'/'<<std::setw(2)<<time.wDay<<'/'<<std::setw(2)<<time.wYear%100<<' '<<std::setw(2)<<time.wHour<<':'<<std::setw(2)<<time.wMinute<<':'<<std::setw(2)<<time.wSecond<<"</Date>\n";
    field("Server",xmlText(server),2);field("Core",xmlText(core()->GetMissionParams()->szIGCStaticFile),2);field("GameOverReason",xmlText(reason),2);
    out<<"</GameInfo>\n<Teams>\n";
    for(const auto& team:results.teams){out<<"  <Team>\n";field("TeamNumber",team.id);field("Name",xmlText(team.name));auto* civ=core()->GetCivilization(team.civilization);field("Faction",xmlText(civ?civ->GetName():""));out<<"  </Team>\n";}
    out<<"</Teams>\n<PlayersScore>\n";
    for(const auto& player:results.players){out<<"  <PlayerScore>\n";field("Name",xmlText(player.name));field("TeamNumber",player.side);field("IsCommander",player.flags&1?"true":"false");field("Rank",player.rank);
        field("Assists",player.assists);field("BaseCaptures",player.baseCaptures);field("BaseKills",player.baseKills);field("Deaths",player.deaths);field("Ejections",player.ejections);field("Kills",player.kills);field("Score",player.score);field("TimePlayed",player.played);out<<"  </PlayerScore>\n";}
    out<<"</PlayersScore>\n</xml>\n";out.close();if(!out)throw std::runtime_error("error saving file "+file.string());
}
}
