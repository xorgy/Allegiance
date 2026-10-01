#pragma once
#include "image_text.hpp"
#include <stdexcept>
#include <vector>
// Frozen ChatListItem constructor wrapping from WinTrek/chatpane.cpp.
// Substitute only the small string/font/point/vector adapters.
namespace ReferenceChatText {
struct ZString {
    std::string text;
    int GetLength()const{return int(text.size());}
    char operator[](int at)const{return text[at];}
    ZString Left(int count)const{return {text.substr(0,count)};}
    ZString RightOf(int offset)const{return {text.substr(std::min(text.size(),size_t(offset))) };}
};
inline ZString operator+(const char* prefix,const ZString& tail){return {std::string(prefix)+tail.text};}
struct Font:ReferenceImageText::Font {
    int GetMaxTextLength(const ZString& str,int width,bool left)const{return ReferenceImageText::Font::GetMaxTextLength({str.text},width,left);}
};
struct Point {int x;int X()const{return x;}};
struct Lines {std::vector<std::string> text;void PushEnd(const char* line){text.emplace_back(line);}void PushEnd(const ZString& line){text.push_back(line.text);}};
inline void ZAssert(bool value){if(!value)throw std::runtime_error("Original ChatListItem short-string assertion");}
inline std::vector<std::string> wrap(std::string value,int width,const Font& font){
    ZString strMsg{std::move(value)};Point ptLineSize{width};Lines m_vMsgLines;
        int nStrLenLeft = strMsg.GetLength();
        int nStrLenLine;

        const Font* pfont = &font;
        int availableWidth = ptLineSize.X() - 6;
        while ((nStrLenLine = pfont->GetMaxTextLength(strMsg, availableWidth, true))
                < nStrLenLeft)
        {
            int nStrLenWordBreak = nStrLenLine;
            while (nStrLenWordBreak > 2 && strMsg[nStrLenWordBreak] != ' ' && strMsg[nStrLenWordBreak-1] != ' ')
                nStrLenWordBreak--;
            if (nStrLenWordBreak != 2)
                nStrLenLine = nStrLenWordBreak;

            if (nStrLenLine <= 2)
            {
                // put a blank line without the character
                nStrLenLine = 3;
                m_vMsgLines.PushEnd("");
            }
            else
            {
                m_vMsgLines.PushEnd(strMsg.Left(nStrLenLine));
            }
            strMsg = "  " + strMsg.RightOf(nStrLenLine);
            nStrLenLeft -= nStrLenLine - 2;
            ZAssert(strMsg.GetLength() == nStrLenLeft);
        }
        m_vMsgLines.PushEnd(strMsg);
    return std::move(m_vMsgLines.text);
}
}
