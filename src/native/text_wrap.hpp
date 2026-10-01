#pragma once
#include <string_view>
#include <vector>
#include <cstddef>
#include "../ClientCore/AuthoredText.h"
namespace Native {
struct WrappedTextLine {size_t offset{},length{};bool continued{};};
template<class Advance> void wrapOriginalChat(std::string_view text,int width,Advance advance,std::vector<WrappedTextLine>& out){
    out.clear();Allegiance::ChatTextLines(text,width,advance,[&](size_t offset,size_t length,bool continued){out.push_back({offset,length,continued});});
}
// Width uses the resident font's advances. Layout happens on message/font
// changes, and spans preserve UTF-8 boundaries without allocating line strings.
template<class Width> void wrapText(std::string_view text,float limit,float indent,
    Width width,std::vector<WrappedTextLine>& out) {
    out.clear();size_t start=0;bool continued=false;
    while(start<text.size()) {
        size_t at=start,end=start,space=text.npos;float used=0;
        float available=limit-(continued?indent:0);
        bool newline=false;
        while(at<text.size()) {
            if(text[at]=='\n'){end=at;newline=true;break;}
            size_t next=at+1;
            while(next<text.size() && (static_cast<unsigned char>(text[next])&0xc0)==0x80)++next;
            float advance=width(text.substr(at,next-at));
            if(used+advance>available && at>start){end=space!=text.npos && space>start?space:at;break;}
            if(text[at]==' ')space=at;
            used+=advance;end=next;at=next;
        }
        out.push_back({start,end-start,continued});
        start=end;
        if(start<text.size() && (text[start]==' ' || text[start]=='\n')){newline=text[start]=='\n';++start;}
        continued=!newline;
    }
    if(text.empty() || text.back()=='\n')out.push_back({text.size(),0,false});
}
}
