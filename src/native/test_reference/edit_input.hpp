#pragma once
#include <string>
// Frozen EditPaneImpl::OnChar from engine/controls.cpp. The portable adapters
// only supply its string, clock, event and repaint dependencies.
namespace ReferenceEditInput {
struct ZString {
    std::string text;
    ZString()=default;ZString(std::string value):text(std::move(value)){}
    ZString(char ch,int length):text(length,ch){}
    int GetLength()const{return int(text.size());}
    ZString LeftOf(int count)const{return text.substr(0,text.size()-count);}
    friend ZString operator+(const ZString& a,const ZString& b){return a.text+b.text;}
};
struct IInputProvider {};
struct KeyState {unsigned char vk;};
struct Time {float value{10};float GetValue()const{return value;}};
struct Event {std::string accepted;void Trigger(const ZString& value){accepted=value.text;}};
struct EditPane {
    enum Type {Normal,Numeric};
    bool m_bReadOnly{};Type m_type{Normal};int m_maxLength{-1};float m_timeLast{};
    ZString m_str;Time time;Event event;Event* m_peventSourceZString{&event};
    Time* GetTime(){return &time;}
    void SetString(ZString value){m_str=std::move(value);}
    void NeedPaint(){}
    bool OnChar(IInputProvider* pprovider, const KeyState& ks)
    {
        if (m_bReadOnly) 
            return true;

        if (ks.vk == 13) {
            m_peventSourceZString->Trigger(m_str);
        } else if (ks.vk == 8) {
            if (m_str.GetLength() > 0) {
                SetString(m_str.LeftOf(1));
                NeedPaint();
            }
        } else {
            char ch = ks.vk;

            //
            // Unblink the cursor
            //

            m_timeLast = GetTime()->GetValue();

            //
            // Only allow numbers in a Numeric edit pane
            //

            if (m_type == Numeric && (ch < '0' || ch > '9' )) {
                return true;
            }

            //
            // Limit the length
            //

            if (m_maxLength != -1 && m_str.GetLength() == m_maxLength) {
                return true;
            }

            //
            // Everythings ok add the character
            //

            SetString(m_str + ZString(ch, 1));
            NeedPaint();
        }

        return true;
    }

};
}
