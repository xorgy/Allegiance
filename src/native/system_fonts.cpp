#include "system_fonts.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#ifdef __APPLE__
namespace Native {std::string systemFontPath(bool bold);}
#elif !defined(_WIN32)
#include <fontconfig/fontconfig.h>
#else
#include <windows.h>
#endif

namespace Native {
SystemFonts::SystemFonts() {
    try {
        if(FT_Init_FreeType(&library_))throw std::runtime_error("FreeType initialization failed");
        for(size_t i=0;i<faces_.size();++i) {
#ifdef _WIN32
            wchar_t directory[MAX_PATH];GetWindowsDirectoryW(directory,MAX_PATH);
            const auto fonts=std::filesystem::path(directory)/"Fonts";
            const char* regular[]={"verdana.ttf","segoeui.ttf","arial.ttf","tahoma.ttf"};
            const char* bold[]={"verdanab.ttf","segoeuib.ttf","arialbd.ttf","tahomabd.ttf"};
            int error=1;
            for(auto name:i?bold:regular) {
                auto file=fonts/name;if(!std::filesystem::is_regular_file(file))continue;
                error=FT_New_Face(library_,file.string().c_str(),0,&faces_[i]);if(!error)break;
            }
            if(error) {
                // Wine and Windows font substitution can provide a usable
                // installed face without a file under the requested name.
                auto dc=CreateCompatibleDC(nullptr);
                auto font=CreateFontW(-12,0,0,0,i?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Verdana");
                auto previous=SelectObject(dc,font);auto bytes=GetFontData(dc,0,0,nullptr,0);
                if(bytes!=GDI_ERROR && bytes<32*1024*1024) {
                    auto& data=systemFontBytes_[i];data.resize(bytes);
                    if(GetFontData(dc,0,0,data.data(),bytes)==bytes)error=FT_New_Memory_Face(library_,data.data(),FT_Long(data.size()),0,&faces_[i]);
                }
                SelectObject(dc,previous);DeleteObject(font);DeleteDC(dc);
            }
#elif defined(__APPLE__)
            auto path=systemFontPath(i!=0);
            int error=FT_New_Face(library_,path.c_str(),0,&faces_[i]);
#else
            FcPattern* query=FcNameParse(reinterpret_cast<const FcChar8*>(i?"Verdana,DejaVu Sans:weight=bold":"Verdana,DejaVu Sans"));
            FcConfigSubstitute(nullptr,query,FcMatchPattern);FcDefaultSubstitute(query);
            FcResult result;FcPattern* match=FcFontMatch(nullptr,query,&result);FcChar8* path=nullptr;
            bool found=match && FcPatternGetString(match,FC_FILE,0,&path)==FcResultMatch;
            int error=found?FT_New_Face(library_,reinterpret_cast<const char*>(path),0,&faces_[i]):1;
            if(match)FcPatternDestroy(match);FcPatternDestroy(query);
#endif
            if(error)throw std::runtime_error("Cannot load a system sans-serif HUD font");
        }
#if !defined(_WIN32) && !defined(__APPLE__)
        FcFini();
#endif
    } catch(...) {
        for(auto face:faces_)if(face)FT_Done_Face(face);
        if(library_)FT_Done_FreeType(library_);
        throw;
    }
}
SystemFonts::~SystemFonts(){for(auto face:faces_)if(face)FT_Done_Face(face);if(library_)FT_Done_FreeType(library_);}
std::shared_ptr<const BitmapFont> SystemFonts::font(const std::string& family,int height,bool bold) {
    for(const auto& cached:fonts_)if(cached.family==family && cached.height==height && cached.bold==bold)return cached.font;
    if(family!="Verdana")throw std::runtime_error("Unsupported authored system font: "+family);
    if(height<1 || height>128)throw std::runtime_error("Invalid authored font cell height");
    auto f=face(bold);FT_Set_Pixel_Sizes(f,0,height);
    // Win32Font's positive height includes leading. Preserve the existing HUD
    // fit, and use these exact metrics during both MDL layout and drawing.
    int pixels=std::max(1,int(std::round(float(height)*height/std::max(1.f,float(f->size->metrics.height)/64))));
    FT_Set_Pixel_Sizes(f,0,pixels);int ascent=int(f->size->metrics.ascender/64);
    auto result=std::make_shared<BitmapFont>();result->height=height;
    unsigned cell=std::max(32u,std::bit_ceil(unsigned(height)));
    for(unsigned ch=0;ch<256;++ch)if(!FT_Load_Char(f,ch,FT_LOAD_DEFAULT))cell=std::max(cell,std::bit_ceil(unsigned(std::max(1,int(f->glyph->advance.x/64)))));
    auto atlas=std::make_shared<ImageData>();atlas->name="hud:"+family+":"+std::to_string(height)+(bold?":bold":"");
    atlas->width=atlas->height=cell*16;atlas->mipmaps=false;atlas->format=ImageFormat::Coverage;atlas->pixels.resize(atlas->byteSize());
    for(unsigned ch=0;ch<256;++ch){if(FT_Load_Char(f,ch,FT_LOAD_RENDER))continue;auto glyph=f->glyph;auto& bitmap=glyph->bitmap;
        unsigned x=(ch%16)*cell,y=(ch/16)*cell;int advance=int(glyph->advance.x/64);
        for(unsigned row=0;row<bitmap.rows;++row)for(unsigned col=0;col<bitmap.width;++col){int gx=int(col)+glyph->bitmap_left,gy=int(row)+ascent-glyph->bitmap_top;
            if(gx>=0 && gx<advance && gy>=0 && gy<height)atlas->pixels[(y+gy)*atlas->width+x+gx]=bitmap.buffer[int(row)*bitmap.pitch+col];}
        result->glyphs[ch]={x,y,advance,height};}
    result->coverage=std::move(atlas);fonts_.push_back({family,height,bold,result});return result;
}
}
