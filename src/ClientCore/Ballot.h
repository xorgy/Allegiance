#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Allegiance {
enum class BallotKind { None,Resign,OfferDraw,AcceptDraw,Mutiny };
inline BallotKind BallotCommand(std::string_view text) {
    auto starts=[&](std::string_view prefix){if(text.size()<prefix.size())return false;
        for(size_t i=0;i<prefix.size();++i){auto ch=text[i];if(ch>='A' && ch<='Z')ch+=('a'-'A');if(ch!=prefix[i])return false;}return true;};
    // FedSrv matches case-insensitive prefixes, not whitespace-delimited words.
    return starts("#resign")?BallotKind::Resign:starts("#draw")?BallotKind::OfferDraw:starts("#mutiny")?BallotKind::Mutiny:BallotKind::None;
}
inline const char* BallotDuplicate(BallotKind kind) {
    return kind==BallotKind::Resign?"Resign vote already in progress...":kind==BallotKind::OfferDraw?"Draw vote already in progress...":"Mutiny vote already in progress...";
}
struct BallotVoter {int pilot{},side{};bool voted{};};
struct BallotTally {int abstaining{},favor{},opposed{};};
// FedSrv/fsmission.cpp's Ballot rules: thirty seconds, the proposer votes yes,
// and each participating side needs more yes than no. Abstentions do not count
// against the proposal. Sorted stable IDs avoid per-vote mission walks and
// object ownership; only the small tally is examined when a ballot settles.
struct Ballot {
    static constexpr double duration=30;
    uint32_t id{};BallotKind kind{};int initiator{-1},side{-1};double expires{};
    std::string name;
    std::vector<BallotVoter> voters;std::vector<BallotTally> tally;int remaining{};
    void prepare(int sides) {
        tally.assign(sides,{});remaining=0;
        std::sort(voters.begin(),voters.end(),[](auto a,auto b){return a.pilot<b.pilot;});
        for(auto& voter:voters){++tally.at(voter.side).abstaining;++remaining;}
    }
    bool cast(int pilot,int currentSide,bool agree) {
        auto voter=std::lower_bound(voters.begin(),voters.end(),pilot,[](const auto& v,int id){return v.pilot<id;});
        if(voter==voters.end() || voter->pilot!=pilot || voter->voted || voter->side!=currentSide)return false;
        voter->voted=true;auto& counts=tally[voter->side];--counts.abstaining;--remaining;
        ++(agree?counts.favor:counts.opposed);return true;
    }
    bool ready(double now)const{return expires<now || !remaining;}
    bool passed()const {
        for(auto& counts:tally)if(counts.abstaining+counts.favor+counts.opposed && counts.favor<=counts.opposed)return false;
        return true;
    }
    std::string proposal()const {
        return name+(kind==BallotKind::Resign?"'s proposal to resign":kind==BallotKind::OfferDraw?"'s proposal to offer a draw":kind==BallotKind::AcceptDraw?"'s offer of a draw":"'s proposal to mutiny");
    }
    std::string prompt()const {
        return name+(kind==BallotKind::Resign?" has proposed resigning.  ":kind==BallotKind::OfferDraw?" has proposed offering a draw.  ":kind==BallotKind::AcceptDraw?" has offered a draw.  ":" has proposed to munity.  ");
    }
    template<class SideName> std::string result(SideName sideName)const {
        std::string text=proposal()+(passed()?" has passed.  ":" has been defeated.  ");bool first=true;
        for(size_t side=0;side<tally.size();++side){auto& c=tally[side];if(!(c.abstaining+c.favor+c.opposed))continue;
            text+=first?"(":", ";first=false;if(kind==BallotKind::AcceptDraw)text+=std::string(sideName(int(side)))+":";
            if(c.favor)text+=" "+std::to_string(c.favor)+" for";
            if(c.opposed)text+=" "+std::to_string(c.opposed)+" against";
            if(c.abstaining)text+=" "+std::to_string(c.abstaining)+" abstained";
        }
        if(!first)text+=")";return text;
    }
};
struct PendingBallot {int id{-1},side{-1};double expires{};std::string text;};
inline std::string BallotInstructions(std::string_view prompt){return std::string(prompt)+"Press [Y] to vote yes, [N] to vote no.";}
}
