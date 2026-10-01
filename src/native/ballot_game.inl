    std::vector<Allegiance::Ballot> ballots;
    std::unordered_map<int,std::deque<Allegiance::PendingBallot>> pendingBallots;
    std::deque<Allegiance::PendingBallot> automaticBallots;
    uint32_t nextBallot{};
    bool castBallot(int id,int pilot,bool agree) {
        if(id<0 || state.complete || mission->GetMissionStage()!=STAGE_STARTED)return false;
        auto* ship=mission->GetShip(ShipID(pilot));if(!ship || !ship->GetSide())return false;
        for(auto& ballot:ballots)if(ballot.id==uint32_t(id) && !(ballot.expires<elapsed)){
            if(!ballot.cast(pilot,ship->GetSide()->GetObjectID(),agree))return false;
            if(auto queue=pendingBallots.find(pilot);queue!=pendingBallots.end()){
                std::erase_if(queue->second,[&](const auto& pending){return pending.id==id;});
                if(queue->second.empty())pendingBallots.erase(queue);
            }
            return true;
        }
        return false;
    }
    void receiveBallot(const Net::Notice& notice) {
        if(!onTeam() || state.complete || notice.sender<0 || notice.color!=uint32_t(player->GetSide()->GetObjectID()) ||
           notice.time+Allegiance::Ballot::duration<elapsed)return;
        if((notice.flags&Net::NoticeHideBallot) && commanders[player->GetSide()->GetObjectID()]==player->GetObjectID()){
            if(gameRole==GameRole::Offline)castBallot(notice.sender,player->GetObjectID(),false);
            else if(gameRole==GameRole::Replica && !submit(Net::Operation::Vote,{notice.sender,0}))
                automaticBallots.push_back({notice.sender,int(notice.color),notice.time+Allegiance::Ballot::duration,{}});
            return;
        }
        auto& queue=pendingBallots[player->GetObjectID()];
        if(std::none_of(queue.begin(),queue.end(),[&](auto& p){return p.id==notice.sender;}))
            queue.push_back({notice.sender,int(notice.color),notice.time+Allegiance::Ballot::duration,Allegiance::BallotInstructions(notice.text)});
    }
    void refreshBallot() {
        if(!automaticBallots.empty())for(auto i=automaticBallots.begin();i!=automaticBallots.end();){
            if(!onTeam() || state.complete || i->side!=player->GetSide()->GetObjectID() || i->expires<elapsed || submit(Net::Operation::Vote,{i->id,0}))i=automaticBallots.erase(i);else ++i;
        }
        if(pendingBallots.empty()){if(state.ballot.id>=0)state.ballot={};return;}
        auto queue=player?pendingBallots.find(player->GetObjectID()):pendingBallots.end();
        if(queue!=pendingBallots.end()){
            auto& q=queue->second;
            while(!q.empty() && (q.front().expires<elapsed || !onTeam() || state.complete || q.front().side!=player->GetSide()->GetObjectID()))q.pop_front();
            if(!q.empty()){if(state.ballot.id!=q.front().id)state.ballot=q.front();return;}
            pendingBallots.erase(queue);
        }
        if(state.ballot.id>=0)state.ballot={};
    }
    void startBallot(Allegiance::BallotKind kind,int initiator,int sideID) {
        using K=Allegiance::BallotKind;
        auto* side=mission->GetSide(SideID(sideID));if(!side)return;
        auto* proposer=kind==K::AcceptDraw?nullptr:mission->GetShip(ShipID(initiator));
        if(kind!=K::AcceptDraw && !proposer)return;
        // IDs use Notice's signed sender slot. Wrap without aliasing an active
        // ballot; expired client prompts live for at most thirty seconds.
        while(std::any_of(ballots.begin(),ballots.end(),[&](const auto& b){return b.id==nextBallot;}))nextBallot=(nextBallot+1)&0x7fffffff;
        Allegiance::Ballot b;b.id=nextBallot;nextBallot=(nextBallot+1)&0x7fffffff;
        b.kind=kind;b.initiator=initiator;b.side=sideID;b.expires=elapsed+Allegiance::Ballot::duration;
        b.name=proposer?proposer->GetName():side->GetName();
        for(auto* link=mission->GetShips()->first();link;link=link->next()){
            auto* ship=link->data();auto* team=ship->GetSide();if(ship->GetPilotType()<c_ptPlayer || !team || team->GetObjectID()<0)continue;
            bool include=kind==K::AcceptDraw?team!=side && team->GetActiveF():team==side;
            if(include)b.voters.push_back({ship->GetObjectID(),team->GetObjectID()});
        }
        b.prepare(mission->GetSides()->n());if(proposer)b.cast(initiator,sideID,true);
        auto prompt=b.prompt();ballots.push_back(std::move(b));auto& stored=ballots.back();
        int old=player->GetObjectID();
        for(auto voter:stored.voters)if(voter.pilot!=initiator || kind==K::AcceptDraw){
            Net::Notice n;n.number=++lastNotice;n.time=elapsed;n.flags=Net::NoticeBallot|(kind==K::Mutiny?Net::NoticeHideBallot:0);
            n.sender=int(stored.id);n.voice=int(kind);n.color=uint32_t(voter.side);Net::text(n.text,prompt);
            if(gameRole==GameRole::Authority){publicationEffectsIndexed=false;networkNotices.push_back({voter.pilot,n});}
            if(activateActor(voter.pilot))receiveBallot(n);
        }
        activateActor(old);
    }
    void proposeBallot(Allegiance::BallotKind kind) {
        if(!onTeam() || !player->GetSide()->GetActiveF())return;
        if(kind==Allegiance::BallotKind::Mutiny && commanders[player->GetSide()->GetObjectID()]==player->GetObjectID())return;
        for(auto& ballot:ballots)if(ballot.kind==kind){SendChat(nullptr,CHAT_INDIVIDUAL_NOFILTER,player->GetObjectID(),NA,Allegiance::BallotDuplicate(kind));return;}
        startBallot(kind,player->GetObjectID(),player->GetSide()->GetObjectID());
    }
    void updateBallots() {
        if(ballots.empty())return;
        if(state.complete){ballots.clear();return;}
        for(size_t i=0;i<ballots.size() && !state.complete;){
            if(!ballots[i].ready(elapsed)){++i;continue;}
            auto ballot=std::move(ballots[i]);ballots.erase(ballots.begin()+i);
            auto text=ballot.result([&](int side){return mission->GetSide(SideID(side))->GetName();});
            bool all=ballot.kind==Allegiance::BallotKind::AcceptDraw;
            SendChat(nullptr,all?CHAT_EVERYONE:CHAT_TEAM,all?NA:ballot.side,NA,text.c_str());
            if(!ballot.passed() || mission->GetMissionStage()!=STAGE_STARTED)continue;
            auto* side=mission->GetSide(SideID(ballot.side));if(!side)continue;
            switch(ballot.kind){
            case Allegiance::BallotKind::Resign:
                deactivateSide(side);settleDeaths();
                if(auto* winner=Allegiance::AlliedSurvivor(mission)){state.complete=true;state.endgame.winner=winner->GetObjectID();state.result=std::string(winner->GetName())+" wins";}
                else {bool active=false;for(auto* l=mission->GetSides()->first();l;l=l->next())active|=l->data()->GetActiveF();
                    if(!active){state.complete=true;state.endgame.winner=NA;state.result="All sides were destroyed";}}break;
            case Allegiance::BallotKind::OfferDraw:
                SendChat(nullptr,CHAT_TEAM,ballot.side,NA,"A draw is being offered to the other teams.");
                startBallot(Allegiance::BallotKind::AcceptDraw,ballot.side,ballot.side);break;
            case Allegiance::BallotKind::AcceptDraw:
                state.complete=true;state.endgame.winner=NA;state.result="The game was declared a draw";break;
            case Allegiance::BallotKind::Mutiny:{
                auto* next=mission->GetShip(ShipID(ballot.initiator));auto* old=mission->GetShip(ShipID(commanders[ballot.side]));
                if(!next || next->GetSide()!=side || next->GetPilotType()<c_ptPlayer || !old || next==old)break;
                setDonation(next,nullptr);
                if(accounts.contains(old->GetObjectID()) && accounts.contains(next->GetObjectID())){auto money=balance(old);balance(old)=0;balance(next)+=money;}
                Allegiance::RedirectDepartingDonations(side->GetShips(),old,next,[&](IshipIGC* donor,IshipIGC* recipient){setDonation(donor,recipient);});
                setDonation(old,next);next->SetWingID(0);changeCommander(side,next);economyDirty=true;updateEconomy();break;
            }
            default:break;
            }
        }
        if(state.complete)ballots.clear();
    }
