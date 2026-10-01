// Frozen badwords.cpp dictionary and substitution from original commit
// 39ef2c072b1eac0e9f97ab1428bc96572cbd5854. Safe unsigned table indexing
// defines the formerly undefined high-byte case; ASCII behavior is unchanged.
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
namespace OriginalChatCensor {
inline std::vector<std::string> words() {
    const char source[]="zone55|anus|a n u s|asshole|a s s h o l e|bastard|bitch|b i t c h|blowjob|b l o w j o b|clit|c l i t|cock|c0ck|cocksucker|c u m|cunt|c u n t|dick|dildo|d i l d o|faggot|f a g g o t|fatass|f uck|fugly|fux0r|fuckyou|fuck_you|fuck_u|fucku|fucka|fuckit|fuckthis|fuckme|fucker|fuckr|fucking|fuckin|fuckn|motherfucker|motherfuck|mutherfucker|fucked|f_u_c|f_ck|f_k|fahq|fck|fkyou|fu_k|fuc|fuck|f u c k|fuhk|fuk|fuq|f__c|f__k|f__u|fuh_q|gay|genital|hitler|jackoff|jism|l3itch|lesbian|masterbat|mofo|nazi|n a z i|niger|nigr|nigga|niggr|nigger|n i g g e r|nutsack|orgy|pecker|penis|p e n i s|phaq|phuc|phuk|phuck|phuq|phvc|phvk|phvq|phallus|pimp|prick|p r i c k|puss|pussy|p u s s y|s.o.b|scrotum|schit|sh1t|shit|s h i t|shlt|shyt|slut|testical|tits|t i t s|vagina|wank|whore|whoar|\x03";std::vector<std::string> result;
    const char* next=std::strchr(source,'|')+1;
    while(*next!=3){const char* end=std::strchr(next,'|');result.emplace_back(next,end-next);next=end+1;}
    std::stable_sort(result.begin(),result.end(),[](auto& a,auto& b){return a.size()>b.size();});return result;
}
inline std::string filter(std::string text) {
    static const auto dictionary=words();static const char convert[]="\0........\t\n..\r.................. !\"#$%&'()*+,-./0123456789:;<=>?@abcdefghijklmnopqrstuvwxyz[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~.................................................................................................................................";
    const char* random="*&^%$#@!!@#$%^&*@$^*!#%&&%#!*^$@";
    std::string lower=text;
    for(auto& c:lower)c=convert[static_cast<unsigned char>(c)];
    for(const auto& word:dictionary) {
        size_t found=lower.find(word);
        while(found!=lower.npos) {
            size_t offset=found;
            for(char c:word){char replacement=random[static_cast<unsigned char>(c)%32];text[offset]=replacement;lower[offset]=replacement;++offset;}
            found=lower.find(word);
        }
    }
    return text;
}
}
