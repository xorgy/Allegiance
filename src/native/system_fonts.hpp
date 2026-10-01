#pragma once
#include "assets.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H

namespace Native {
// One resident pair of system faces serves both authored cell-height fonts
// and the optional modern UI's pixel-height atlas. Assets creates it lazily.
class SystemFonts {
public:
    SystemFonts();
    ~SystemFonts();
    SystemFonts(const SystemFonts&)=delete;
    SystemFonts& operator=(const SystemFonts&)=delete;
    FT_Face face(bool bold)const{return faces_[bold];}
    std::shared_ptr<const BitmapFont> font(const std::string& family,int height,bool bold);
private:
    FT_Library library_{};
    std::array<FT_Face,2> faces_{};
    std::array<std::vector<uint8_t>,2> systemFontBytes_;
    struct Cached {std::string family;int height;bool bold;std::shared_ptr<const BitmapFont> font;};
    std::vector<Cached> fonts_;
};
}
