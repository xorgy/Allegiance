#include "assets.hpp"
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Native {
std::shared_ptr<ImageData> decodeImage(const std::filesystem::path& path) {
    const auto name=path.string();
    auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(name.data()),name.size(),false);
    if(!url)throw std::runtime_error("Cannot create image URL");
    auto source=CGImageSourceCreateWithURL(url,nullptr);CFRelease(url);
    if(!source)throw std::runtime_error("ImageIO cannot open "+name);
    auto image=CGImageSourceCreateImageAtIndex(source,0,nullptr);CFRelease(source);
    if(!image)throw std::runtime_error("ImageIO cannot decode "+name);
    struct Cleanup {CGImageRef image;~Cleanup(){CGImageRelease(image);}} cleanup{image};
    auto result=std::make_shared<ImageData>();result->name=name;
    result->width=CGImageGetWidth(image);result->height=CGImageGetHeight(image);
    if(!result->width || !result->height || result->width>16384 || result->height>16384)throw std::runtime_error("Invalid image dimensions");
    result->pixels.resize(result->byteSize());
    auto alpha=CGImageGetAlphaInfo(image);auto order=CGImageGetBitmapInfo(image)&kCGBitmapByteOrderMask;
    const auto bits=CGImageGetBitsPerPixel(image);
    // ImageIO exposes authored PNG samples directly. This preserves straight
    // alpha (including faint RGB), avoids a second color conversion and writes
    // only the final RGBA upload buffer. Rows retain the file's top-down order.
    if(CGColorSpaceGetModel(CGImageGetColorSpace(image))==kCGColorSpaceModelRGB && CGImageGetBitsPerComponent(image)==8 &&
       (order==kCGBitmapByteOrderDefault || order==kCGBitmapByteOrder32Big) &&
       ((bits==32 && (alpha==kCGImageAlphaLast || alpha==kCGImageAlphaNoneSkipLast)) || (bits==24 && alpha==kCGImageAlphaNone))) {
        auto data=CGDataProviderCopyData(CGImageGetDataProvider(image));
        const auto stride=CGImageGetBytesPerRow(image);const auto required=stride*(result->height-1)+result->width*(bits/8);
        if(data && CFDataGetLength(data)>=required) {
            const auto* bytes=CFDataGetBytePtr(data);
            for(uint32_t y=0;y<result->height;++y) {
                const auto* src=bytes+y*stride;auto* dst=result->pixels.data()+size_t(y)*result->width*4;
                if(bits==32 && alpha==kCGImageAlphaLast)std::memcpy(dst,src,result->width*4);
                else for(uint32_t x=0;x<result->width;++x){std::memcpy(dst+x*4,src+x*(bits/8),3);dst[x*4+3]=255;}
            }
            CFRelease(data);return result;
        }
        if(data)CFRelease(data);
    }
    auto color=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    auto context=CGBitmapContextCreate(result->pixels.data(),result->width,result->height,8,result->width*4,color,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(color);
    if(!context)throw std::runtime_error("Cannot create native image converter");
    CGContextDrawImage(context,CGRectMake(0,0,result->width,result->height),image);CGContextRelease(context);
    for(size_t pixel=0;pixel<result->pixels.size();pixel+=4) {
        const auto a=result->pixels[pixel+3];
        if(a && a<255)for(size_t c=0;c<3;++c)result->pixels[pixel+c]=uint8_t(std::min(255u,(unsigned(result->pixels[pixel+c])*255+a/2)/a));
    }
    return result;
}
}
