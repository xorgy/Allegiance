#pragma once
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace Allegiance {
// TrekResources::GetFont: unknown sizes use the original small face.
inline const char* ChatFontName(int size,bool bold=false) {
    return size==2?(bold?"largeBoldFont":"largeFont"):size==3?(bold?"hugeBoldFont":"hugeFont"):(bold?"smallBoldFont":"smallFont");
}
// FontImpl's inline color controls occupy ten/two bytes, including the
// separator byte. Extent/drawing skip them; GetMaxTextLength still fits their
// individual bytes. Keep those intentionally different original policies.
inline std::size_t TextColorBytes(std::string_view text) {
    if(text.empty())return 0;
    auto byte=static_cast<unsigned char>(text.front());
    return byte==0x81?std::min(text.size(),std::size_t(10)):byte==0x82?std::min(text.size(),std::size_t(2)):0;
}
struct InlineTextColor {std::array<uint8_t,4> rgba{};bool valid{},reset{};};
inline bool ConsumeTextColor(std::string_view& text,InlineTextColor& color) {
    auto count=TextColorBytes(text);if(!count)return false;
    color={};color.reset=static_cast<unsigned char>(text.front())==0x82;color.valid=color.reset;
    if(!color.reset && count==10){
        auto hex=[](unsigned char c)->int {return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;};
        color.valid=true;
        for(int i=0;i<4;++i){int high=hex(text[2+i*2]),low=hex(text[3+i*2]);if(high<0 || low<0){color.valid=false;break;}color.rgba[i]=uint8_t((high<<4)|low);}
    }
    text.remove_prefix(count);return true;
}
template<class Advance> float TextExtent(std::string_view text,Advance advance) {
    float width=0;while(!text.empty()){if(auto count=TextColorBytes(text))text.remove_prefix(count);else {width+=advance(static_cast<unsigned char>(text.front()));text.remove_prefix(1);}}return width;
}
// Original font fitting is byte based and includes a glyph on an exact fit.
// Keep suffix clipping distinct from StringImage's word-break policy.
template<class Advance> std::size_t FitText(std::string_view text,int width,bool left,Advance advance) {
    float used=0;for(std::size_t i=0;i<text.size();++i){used+=advance(static_cast<unsigned char>(text[left?i:text.size()-i-1]));if(used>width)return i;}return text.size();
}
template<class Advance> std::string_view ClipPaneText(std::string_view text,int width,int justify,Advance advance) {
    bool left=justify!=2 && justify!=3;auto count=FitText(text,width,left,advance);
    return left?text.substr(0,count):text.substr(text.size()-count);
}
struct ImageTextLine {std::size_t length{},consumed{};};
// StringImage::BreakLine, without temporary strings. Preserve its separator
// consumption, including the original hard-break and leading-space behavior.
template<class Fit> ImageTextLine BreakImageLine(std::string_view text,int width,Fit fit) {
    auto newline=text.find('\n');if(newline==text.npos)newline=text.size();
    auto count=std::min(std::size_t(fit(text,width)),text.size());
    if(count>=newline)count=newline;
    else if(count!=text.size())for(std::size_t i=count;i>0;){--i;if(text[i]==' '){while(i>0){if(text[i]!=' '){count=i+1;break;}--i;}break;}}
    return {count,count<text.size()?count+1:count};
}
template<class Advance> ImageTextLine ImageLine(std::string_view text,int width,Advance advance) {
    return BreakImageLine(text,width,[&](std::string_view line,int limit){return FitText(line,limit,true,advance);});
}
template<class Advance> std::size_t ImageTextLines(std::string_view text,int width,Advance advance) {
    std::size_t count=0;while(!text.empty()){auto line=ImageLine(text,width,advance);text.remove_prefix(line.consumed);++count;}return count;
}
// ChatListItem fits raw bytes, keeps the spaces at its word break and prepends
// two spaces on subsequent rows. Describe the original virtual prefixed string
// with spans instead of copying the remaining message after every break.
template<class Advance,class Emit> void ChatTextLines(std::string_view text,int width,Advance advance,Emit emit) {
    std::size_t offset=0;bool continued=false;
    for(;;){
        auto prefix=continued?std::size_t(2):0;auto length=prefix+text.size()-offset;
        auto byte=[&](std::size_t at){return at<prefix?static_cast<unsigned char>(' '):static_cast<unsigned char>(text[offset+at-prefix]);};
        float used=0;std::size_t fit=0;for(;fit<length;++fit){used+=advance(byte(fit));if(used>width)break;}
        if(fit==length){emit(offset,text.size()-offset,continued);return;}
        auto wordBreak=fit;while(wordBreak>2 && byte(wordBreak)!=' ' && byte(wordBreak-1)!=' ')--wordBreak;
        if(wordBreak!=2)fit=wordBreak;
        bool blank=fit<=2;if(blank)fit=3;
        auto consumed=std::min(text.size()-offset,fit-prefix);
        emit(offset,blank?0:consumed,blank?false:continued);offset+=consumed;continued=true;
        // Unlike the legacy unchecked short-string case, always terminate for
        // zero/negative widths and messages shorter than the three-byte skip.
        if(!consumed){emit(offset,text.size()-offset,true);return;}
    }
}
// EditPane reserves the width of 'o' for its end cursor, retaining a suffix
// while focused/editable and a prefix otherwise. Focus does not restart blink.
template<class Advance> std::string_view ClipEditText(std::string_view text,int width,bool focused,bool readOnly,Advance advance,int cursorWidth=-1) {
    if(cursorWidth<0)cursorWidth=int(advance('o'));
    return ClipPaneText(text,width-(focused && !readOnly?cursorWidth:0),focused && !readOnly?2:0,advance);
}
inline bool EditCursorVisible(double time,double lastTyped) {
    return std::fmod(time-lastTyped,1.)<.5;
}
// SDL batches text input. Process the batch as original individual character
// events: accept a fitting prefix and ignore filtered characters. Preserve
// valid native UTF-8 characters at the byte capacity instead of splitting one.
inline std::size_t EditInputBytes(std::string_view text) {
    if(text.empty())return 0;auto first=static_cast<unsigned char>(text.front());
    std::size_t count=first>=0xc2 && first<=0xdf?2:first>=0xe0 && first<=0xef?3:first>=0xf0 && first<=0xf4?4:1;
    if(count>text.size())return 1;
    for(std::size_t i=1;i<count;++i)if((static_cast<unsigned char>(text[i])&0xc0)!=0x80)return 1;
    return count;
}
template<class Accept> void AppendEditInput(std::string& text,std::string_view input,std::size_t capacity,Accept accept) {
    while(!input.empty()){
        auto count=EditInputBytes(input);if(accept(static_cast<unsigned char>(input.front()))){if(text.size()>=capacity || count>capacity-text.size())break;text.append(input.substr(0,count));}input.remove_prefix(count);
    }
}
inline void AppendEditInput(std::string& text,std::string_view input,std::size_t capacity) {
    AppendEditInput(text,input,capacity,[](unsigned char c){return c>=32 && c!=127;});
}
inline void EraseEditCharacter(std::string& text) {
    if(text.empty())return;auto at=text.size()-1;while(at && (static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;text.resize(at);
}
}
