# Names are shared with the Linux pkg-config path so consumers stay identical.
find_package(SDL3 CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(lz4 CONFIG REQUIRED)
unset(FFMPEG_FOUND CACHE) # vcpkg's find module caches a partially populated result.
find_package(FFMPEG REQUIRED)
function(native_dependency name target)
    add_library(PkgConfig::${name} INTERFACE IMPORTED)
    set_target_properties(PkgConfig::${name} PROPERTIES INTERFACE_LINK_LIBRARIES ${target})
endfunction()
native_dependency(SDL3 SDL3::SDL3)
native_dependency(OPENAL OpenAL::OpenAL)
native_dependency(LZ4 lz4::lz4)
native_dependency(FONTS Freetype::Freetype)
add_library(PkgConfig::FFMPEG INTERFACE IMPORTED)
set_target_properties(PkgConfig::FFMPEG PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${FFMPEG_LIBRARIES}")
