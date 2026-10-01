#pragma once
#include "image_text.hpp"
// Frozen EditPaneImpl::UpdateStrings from src/engine/controls.cpp. Only the
// small string/font/size adapters are substituted for a portable comparison.
namespace ReferenceEditText {
struct ZString {
    std::string text;
    ZString()=default;
    ZString(std::string value):text(std::move(value)){}
    ZString(char character,int length):text(length,character){}
    int GetLength()const{return int(text.size());}
    ZString Left(int count)const{return text.substr(0,count);}
    ZString Right(int count)const{return text.substr(text.size()-count);}
};
struct Font:ReferenceImageText::Font {
    int GetMaxTextLength(const ZString& text,int width,bool left)const{return ReferenceImageText::Font::GetMaxTextLength({text.text},width,left);}
};
struct Point {int width;int X()const{return width;}};
struct EditPane {
    enum Type {Normal,Password};
    ZString m_str,m_strDisplay,m_strClipped;
    Type m_type{Normal};bool m_bFocus{},m_bReadOnly{};int m_xsizeCursor{},width{};const Font* m_pfont{};
    Point GetSize()const{return {width};}
    void UpdateStrings()
    {
        if (m_type == Password) {
            m_strDisplay = ZString('*', m_str.GetLength());
        } else {
            m_strDisplay = m_str;
        }

        if (m_bFocus && !m_bReadOnly) {
            m_strClipped =
                m_strDisplay.Right(
                    m_pfont->GetMaxTextLength(
                        m_strDisplay,
                        GetSize().X() - m_xsizeCursor,
                        false
                    )
                );
        } else {
            m_strClipped =
                m_strDisplay.Left(
                    m_pfont->GetMaxTextLength(
                        m_strDisplay,
                        GetSize().X(),
                        true
                    )
                );
        }
    }

};
}
