// WinTrek/appmsg.cpp's personal economy feedback, independent of transport/UI.
#pragma once
namespace Allegiance {
template<class Post>
void PaydayFeedback(int share,int received,const char* recipient,Post post) {
    if(recipient)post(false,"You donated your payday of $%d to %s.",share,recipient);
    else if(received==share)post(false,"You received a payday of $%d.",share);
    else post(false,"You received $%d in pay and donations.",received);
}
template<class Post>
void CashRewardFeedback(int amount,const char* recipient,Post post) {
    if(recipient)post(false,"You donated your reward of $%d to %s.",amount,recipient);
    else post(false,"You received a reward of $%d.",amount);
}
template<class Donor,class Previous,class Recipient,class Investor>
void IncomeDonationFeedback(const char* donor,const char* previous,const char* recipient,
                            Donor by,Previous old,Recipient to,Investor investor) {
    if(recipient)by(true,"You have started donating your income to %s",recipient);
    else by(true,"You have stopped donating your income to %s",previous);
    if(recipient){to(true,"%s is now donating to you",donor);investor();}
    if(previous)old(true,"%s has stopped donating to you",donor);
}
template<class Post>
void TeamLeaderFeedback(bool self,const char* leader,Post post) {
    if(self)post(true,"You have become the team leader");
    else post(true,"%s has become the team leader",leader);
}
}
