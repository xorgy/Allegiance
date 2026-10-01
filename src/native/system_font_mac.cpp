#include <CoreText/CoreText.h>
#include <stdexcept>
#include <string>

namespace Native {
// Keep Apple framework types in this translation unit: MacTypes' global Point
// and Rect names conflict with the original simulation's geometry types.
std::string systemFontPath(bool bold) {
    auto font=CTFontCreateWithName(CFSTR("Verdana"),12,nullptr);
    if(bold){auto copy=CTFontCreateCopyWithSymbolicTraits(font,12,nullptr,kCTFontBoldTrait,kCTFontBoldTrait);if(copy){CFRelease(font);font=copy;}}
    auto url=static_cast<CFURLRef>(CTFontCopyAttribute(font,kCTFontURLAttribute));
    auto path=url?CFURLCopyFileSystemPath(url,kCFURLPOSIXPathStyle):nullptr;
    char file[4096]{};
    bool found=path && CFStringGetCString(path,file,sizeof(file),kCFStringEncodingUTF8);
    if(path)CFRelease(path);if(url)CFRelease(url);CFRelease(font);
    if(!found)throw std::runtime_error("Cannot locate system HUD font");
    return file;
}
}
