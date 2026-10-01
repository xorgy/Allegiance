#pragma once
#include <cstring>

namespace Allegiance {
// Original display dictionary and longest-first substitution. Callers prepare
// the word pointers once, and supply scratch storage of strlen(text)+1 bytes.
static const char ChatCensorWords[] = "zone55|anus|a n u s|asshole|a s s h o l e|bastard|bitch|b i t c h|blowjob|b l o w j o b|clit|c l i t|cock|c0ck|cocksucker|c u m|cunt|c u n t|dick|dildo|d i l d o|faggot|f a g g o t|fatass|f uck|fugly|fux0r|fuckyou|fuck_you|fuck_u|fucku|fucka|fuckit|fuckthis|fuckme|fucker|fuckr|fucking|fuckin|fuckn|motherfucker|motherfuck|mutherfucker|fucked|f_u_c|f_ck|f_k|fahq|fck|fkyou|fu_k|fuc|fuck|f u c k|fuhk|fuk|fuq|f__c|f__k|f__u|fuh_q|gay|genital|hitler|jackoff|jism|l3itch|lesbian|masterbat|mofo|nazi|n a z i|niger|nigr|nigga|niggr|nigger|n i g g e r|nutsack|orgy|pecker|penis|p e n i s|phaq|phuc|phuk|phuck|phuq|phvc|phvk|phvq|phallus|pimp|prick|p r i c k|puss|pussy|p u s s y|s.o.b|scrotum|schit|sh1t|shit|s h i t|shlt|shyt|slut|testical|tits|t i t s|vagina|wank|whore|whoar|\x03";
static const char ChatLowerCase[] = "\0........\t\n..\r.................. !\"#$%&'()*+,-./0123456789:;<=>?@abcdefghijklmnopqrstuvwxyz[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~.................................................................................................................................";
inline void CensorChatInPlace(char* text,char* lower,const char* const* words,int count) {
    static const char obscure[]="*&^%$#@!!@#$%^&*@$^*!#%&&%#!*^$@";
    std::strcpy(lower,text);
    for(char* p=lower;*p;++p)*p=ChatLowerCase[static_cast<unsigned char>(*p)];
    for(int i=0;i<count;++i) {
        char* found=std::strstr(lower,words[i]);
        while(found) {
            const char* word=words[i];char* output=text+(found-lower);
            while(*word) {char replacement=obscure[static_cast<unsigned char>(*word++)%32];*output++=replacement;*found++=replacement;}
            found=std::strstr(lower,words[i]);
        }
    }
}
}
