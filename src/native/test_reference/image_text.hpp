#pragma once
#include <array>
#include <string>
// Frozen StringImage::BreakLine from src/engine/image.cpp at the original
// checkpoint. Only its small string/font adapters are substituted here.
namespace ReferenceImageText {
struct ZString {
    std::string text;
    int GetLength()const{return int(text.size());}
    int Find(const char* what)const{auto at=text.find(what);return at==text.npos?-1:int(at);}
    char operator[](int index)const{return text[index];}
    ZString Left(int count)const{return {text.substr(0,count)};}
    ZString RightOf(int offset)const{return {text.substr(offset)};}
};
struct Font {
    std::array<float,256> advances{};
    int GetMaxTextLength(const ZString& str,int width,bool left)const {
        float used=0;for(int i=0;i<str.GetLength();++i){used+=advances[static_cast<unsigned char>(str[left?i:str.GetLength()-i-1])];if(used>width)return i;}return str.GetLength();
    }
};
struct StringImage {
    const Font* m_pfont;
    ZString BreakLine(ZString& str, int width)
    {
        ZString strSave = str;

        //
        // Search forward for any new lines
        //

        int indexNewLine = str.Find("\n");
        if (indexNewLine == -1) {
            indexNewLine = str.GetLength();
        }

        //
        // Figure out how many characters fit on the line
        //

        int nchar = m_pfont->GetMaxTextLength(str, width, true);

        //
        // Chop off at the newline
        //

        if (nchar >= indexNewLine) {
            nchar = indexNewLine;
        } else {
            //
            // Search backwards for the first space
            //

            if (nchar != str.GetLength()) {
                for (int index = nchar - 1; index >= 0; index--) {
                    if (str[index] == ' ') {
                        while (index > 0) {
                            if (str[index] != ' ') {
                                nchar = index + 1;
                                break;
                            }
                            index--;
                        }
                        break;
                    }
                }
            }
        }

        if (nchar < str.GetLength()) {
            str = strSave.RightOf(nchar + 1);
            return strSave.Left(nchar);
        } else {
            //
            // remove any leading spaces from the next line
            //

            str = strSave.RightOf(nchar);

            int length = str.GetLength();
            int index = 0;

            while (index < length && str[index] == ' ') {
                index++;
            }

            if (index != 0) {
                str = str.RightOf(index);
            }

            return strSave.Left(nchar);
        }
    }
};
}
