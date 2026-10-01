#include "original_cursor.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace Native {
size_t OriginalCursor::index(std::string_view name) {
    auto i=std::find(originalCursorNames.begin(),originalCursorNames.end(),name);
    return i==originalCursorNames.end()?0:size_t(i-originalCursorNames.begin());
}
OriginalCursor::OriginalCursor(Assets& assets) {
    SDL_SetHint("SDL_MOUSE_DPI_SCALE_CURSORS","0");
    OriginalUi original(assets);
    for(size_t i=0;i<art_.size();++i) {
        auto controls=original.imageLayout("cursor",std::string(originalCursorNames[i]));
        if(controls.size()!=1 || !controls[0].image)throw std::runtime_error("Invalid original cursor artwork");
        art_[i]=std::move(controls[0]);
    }
}
OriginalCursor::~OriginalCursor() {
    if(selected_ && SDL_GetCursor()==selected_)SDL_SetCursor(SDL_GetDefaultCursor());
    for(auto& variants:cursors_)for(auto* cursor:variants)if(cursor)SDL_DestroyCursor(cursor);
}
const OriginalControl& OriginalCursor::artwork(std::string_view name)const{return art_[index(name)];}
bool OriginalCursor::set(std::string_view name,int scale,float pixelDensity) {
    scale=std::clamp(scale,1,8);int density=1;
#ifndef _WIN32
    // Cocoa NSCursor and Wayland cursor surfaces use window coordinates. Their
    // base image establishes logical size; alternate images supply exact device
    // pixels. Fractional or incompatible densities need the ordered GPU fallback
    // to avoid platform filtering or doubling an already enlarged cursor.
    const char* driver=SDL_GetCurrentVideoDriver();
    if(driver && (!std::strcmp(driver,"cocoa") || !std::strcmp(driver,"wayland"))) {
        if(!std::isfinite(pixelDensity) || pixelDensity<1 || pixelDensity>8)return false;
        density=int(std::round(pixelDensity));
        if(std::abs(pixelDensity-density)>.0001f || scale%density)return false;
    }
#endif
    auto i=index(name);int variant=(density-1)*8+scale-1;auto& cursor=cursors_[i][variant];
    if(!cursor) {
        if(attempted_[i][variant])return false;attempted_[i][variant]=true;
        const auto& c=art_[i];const auto& image=*c.image;
        auto surfaceFor=[&](int factor) {
            auto surface=SDL_CreateSurface(image.width*factor,image.height*factor,SDL_PIXELFORMAT_RGBA32);
            if(surface)for(uint32_t y=0;y<image.height;++y)for(int sy=0;sy<factor;++sy) {
                auto* row=static_cast<uint8_t*>(surface->pixels)+(y*factor+sy)*surface->pitch;
                for(uint32_t x=0;x<image.width;++x)for(int sx=0;sx<factor;++sx)
                    std::memcpy(row+(x*factor+sx)*4,image.pixels.data()+(y*image.width+x)*4,4);
            }
            return surface;
        };
        int logicalScale=scale/density;auto surface=surfaceFor(logicalScale);if(!surface)return false;
        if(density>1) {
            auto alternate=surfaceFor(scale);bool added=alternate && SDL_AddSurfaceAlternateImage(surface,alternate);
            SDL_DestroySurface(alternate);if(!added){SDL_DestroySurface(surface);return false;}
        }
        cursor=SDL_CreateColorCursor(surface,int(-c.bounds.x)*logicalScale,int(-c.bounds.y)*logicalScale);
        SDL_DestroySurface(surface);if(!cursor)return false;
    }
    if(selected_==cursor && SDL_GetCursor()==cursor)return true;
    if(!SDL_SetCursor(cursor))return false;selected_=cursor;return true;
}
}
