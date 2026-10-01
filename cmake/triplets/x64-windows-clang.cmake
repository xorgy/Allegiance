set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
if(PORT MATCHES "^(openal-soft|ffmpeg)$")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
# vcpkg identifies desktop Windows by an empty system name. The chainloaded
# toolchain sets CMAKE_SYSTEM_NAME; spelling Windows here selects no platform.
set(VCPKG_LOAD_VCVARS_ENV OFF)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_ENV_PASSTHROUGH XWIN_ROOT)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchain-windows-clang.cmake")
