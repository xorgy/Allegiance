# LLVM requires consistent unit splitting across every ThinLTO input when
# whole-program vtable optimization is enabled for the native core below.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    add_compile_options($<$<CONFIG:Release>:-fsplit-lto-unit>)
endif()

find_package(Threads REQUIRED)
add_library(allegiance_affinity STATIC thread_affinity.cpp)
target_compile_features(allegiance_affinity PUBLIC cxx_std_17)
target_link_libraries(allegiance_affinity PUBLIC Threads::Threads)
add_executable(allegiance-affinity-check thread_affinity_check.cpp)
target_link_libraries(allegiance-affinity-check PRIVATE allegiance_affinity)
add_test(NAME native-affinity COMMAND allegiance-affinity-check)
if(WIN32)
    set(USE_CRYPTO BCrypt CACHE STRING "GNS crypto backend")
else()
    find_package(OpenSSL REQUIRED)
endif()
find_package(GameNetworkingSockets CONFIG QUIET)
if(NOT GameNetworkingSockets_FOUND)
    include(FetchContent)
    set(BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(USE_STEAMWEBRTC OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(gns GIT_REPOSITORY https://github.com/ValveSoftware/GameNetworkingSockets.git
        GIT_TAG 0abe0ed1fe318c197b43326d4fbb03b04517c7fb GIT_SUBMODULES "")
    FetchContent_MakeAvailable(gns)
    if(NOT TARGET GameNetworkingSockets::GameNetworkingSockets_s)
        add_library(GameNetworkingSockets::GameNetworkingSockets_s ALIAS GameNetworkingSockets_s)
    endif()
endif()

# Build identity includes all simulation/policy and network source. Platform
# build flags and paths are deliberately excluded: Linux and Windows clients
# of the same revision must connect. The runtime identity also checks wire ABI.
file(GLOB_RECURSE NETWORK_IDENTITY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.inl" "${CMAKE_CURRENT_SOURCE_DIR}/../Igc/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Igc/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../ClientCore/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../zlib/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/../zlib/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../_Utility/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/../_Utility/*.hxx" "${CMAKE_CURRENT_SOURCE_DIR}/../_Utility/*.cpp")
file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" NATIVE_BUILD_RULES_HASH)
list(APPEND NETWORK_IDENTITY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../WinTrek/mappvmaker.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../WinTrek/mappvmaker.h")
set(IDENTITY_INPUT "${NATIVE_BUILD_RULES_HASH}")
foreach(SOURCE ${NETWORK_IDENTITY_SOURCES})
    file(SHA256 "${SOURCE}" SOURCE_HASH)
    string(APPEND IDENTITY_INPUT "${SOURCE_HASH}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${NETWORK_IDENTITY_SOURCES})
string(SHA256 NETWORK_BUILD "${IDENTITY_INPUT}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/network_build.hpp"
    CONTENT "#define ALLEGIANCE_NETWORK_BUILD \"${NETWORK_BUILD}\"\n")

# The native client shares the original simulation, math and collision code.
# Platform definitions here are private to the legacy core, not the renderer.
file(GLOB IGC_SOURCES CONFIGURE_DEPENDS ../Igc/*.cpp)
list(FILTER IGC_SOURCES EXCLUDE REGEX "/private.cpp$")
add_library(allegiance_core STATIC
    ${IGC_SOURCES}
    ../zlib/color.cpp ../zlib/matrix.cpp ../zlib/matrix2.cpp
    ../zlib/orientation.cpp ../zlib/quaternion.cpp ../zlib/vector.cpp
    ../zlib/zmath.cpp ../zlib/zstring.cpp ../zlib/ztime.cpp ../zlib/tlist.cpp
    ../_Utility/Utility.cpp ../_Utility/HitTest.cpp ../_Utility/KDroot.cpp ../_Utility/KDnode.cpp
    ../_Utility/Endpoint.cpp ../_Utility/CollisionQueue.cpp ../_Utility/CollisionEntry.cpp
    ../WinTrek/mappvmaker.cpp
    legacy.cpp)
target_compile_features(allegiance_core PUBLIC cxx_std_17)
target_include_directories(allegiance_core PUBLIC . ../zlib ../Igc ../_Utility)
target_compile_definitions(allegiance_core PUBLIC
    ALLEGIANCE_NATIVE NO_STEAM __MODULE__="native" ZLIB_SKIP_NEW_REDEF)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(MSVC)
        set(NATIVE_FLOAT_OPTIONS /clang:-ffp-contract=off)
    else()
        set(NATIVE_FLOAT_OPTIONS -ffp-contract=off)
    endif()
    target_compile_options(allegiance_core PRIVATE ${NATIVE_FLOAT_OPTIONS})
    add_compile_options(${NATIVE_FLOAT_OPTIONS})
endif()
if(MSVC)
    set(NATIVE_FORCE_INCLUDE /FI${CMAKE_CURRENT_SOURCE_DIR}/legacy.hpp)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
else()
    set(NATIVE_FORCE_INCLUDE -include ${CMAKE_CURRENT_SOURCE_DIR}/legacy.hpp)
endif()
target_compile_options(allegiance_core PRIVATE ${NATIVE_FORCE_INCLUDE}
    -Wno-unknown-pragmas -Wno-write-strings -Wno-multichar)
target_link_libraries(allegiance_core PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(allegiance_core PUBLIC winmm oleaut32)
endif()

# Only the statically linked native core has a closed implementation set.
# Keep external platform/transport interfaces and Windows builds unaffected.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    target_compile_options(allegiance_core PRIVATE
        $<$<CONFIG:Release>:-fwhole-program-vtables>
        $<$<CONFIG:Release>:-fvisibility=hidden>)
endif()

add_executable(allegiance-object-pool-check object_pool_check.cpp)
target_compile_features(allegiance-object-pool-check PRIVATE cxx_std_17)
target_link_libraries(allegiance-object-pool-check PRIVATE Threads::Threads)
add_test(NAME native-object-pool COMMAND allegiance-object-pool-check)

add_executable(allegiance-core-check core_check.cpp)
target_link_libraries(allegiance-core-check PRIVATE allegiance_core)
add_executable(allegiance-dynamic-check dynamic_check.cpp)
target_compile_features(allegiance-dynamic-check PRIVATE cxx_std_20)
target_link_libraries(allegiance-dynamic-check PRIVATE allegiance_assets allegiance_core)

find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
add_executable(allegiance-frustum-check frustum_check.cpp)
target_compile_features(allegiance-frustum-check PRIVATE cxx_std_17)
target_link_libraries(allegiance-frustum-check PRIVATE glm::glm)
add_test(NAME native-frustum COMMAND allegiance-frustum-check)
if(WIN32)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/native-windows-dependencies.cmake)
else()
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(OPENAL REQUIRED IMPORTED_TARGET openal)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
if(APPLE)
    pkg_check_modules(FONTS REQUIRED IMPORTED_TARGET freetype2)
else()
    pkg_check_modules(FONTS REQUIRED IMPORTED_TARGET freetype2 fontconfig)
endif()
pkg_check_modules(LZ4 REQUIRED IMPORTED_TARGET liblz4)
endif()
add_library(allegiance_network STATIC network_records.cpp network_codec.cpp network_arena.cpp network_transport.cpp endpoint_resolver.cpp hostname_transport.cpp server_browser.cpp lan_discovery.cpp public_directory.cpp)
target_compile_features(allegiance_network PUBLIC cxx_std_20)
target_include_directories(allegiance_network PUBLIC . PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(allegiance_network PUBLIC allegiance_affinity GameNetworkingSockets::GameNetworkingSockets_s PkgConfig::LZ4)
add_executable(allegiance-hostname-check hostname_check.cpp)
target_link_libraries(allegiance-hostname-check PRIVATE allegiance_network)
add_test(NAME native-hostnames COMMAND allegiance-hostname-check)
if(WIN32)
    target_link_libraries(allegiance_network PRIVATE bcrypt ws2_32 iphlpapi)
else()
    target_link_libraries(allegiance_network PRIVATE OpenSSL::Crypto)
endif()
if(APPLE)
    set(NATIVE_IMAGE_DECODER media_mac.cpp)
    enable_language(OBJCXX)
    set(NATIVE_AUDIO_DECODER audio_decode_mac.mm)
    set_source_files_properties(audio_decode_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc" COMPILE_FLAGS "-std=c++20")
else()
    set(NATIVE_IMAGE_DECODER media.cpp)
    set(NATIVE_AUDIO_DECODER audio_decode.cpp)
endif()
add_library(allegiance_assets STATIC assets.cpp system_fonts.cpp ${NATIVE_IMAGE_DECODER} primitives.cpp quick_chat.cpp chat_censor.cpp ui_script.cpp original_ui.cpp hud_program.cpp)
target_compile_features(allegiance_assets PUBLIC cxx_std_20)
target_include_directories(allegiance_assets PUBLIC .)
target_link_libraries(allegiance_assets PUBLIC allegiance_affinity glm::glm PkgConfig::FONTS)
if(APPLE)
    target_link_libraries(allegiance_assets PRIVATE "-framework ImageIO" "-framework CoreGraphics" "-framework CoreFoundation")
else()
    target_link_libraries(allegiance_assets PUBLIC PkgConfig::FFMPEG)
endif()
if(WIN32)
    target_link_libraries(allegiance_assets PRIVATE advapi32 gdi32)
endif()
add_executable(allegiance-assets-check assets_check.cpp)
target_link_libraries(allegiance-assets-check PRIVATE allegiance_assets)
add_executable(allegiance-ui-script-check ui_script_check.cpp)
target_link_libraries(allegiance-ui-script-check PRIVATE allegiance_assets)
add_executable(allegiance-core-catalog-check core_catalog_check.cpp settings.cpp)
target_link_libraries(allegiance-core-catalog-check PRIVATE allegiance_frontend)
add_executable(allegiance-original-dialogs-check original_dialogs_check.cpp settings.cpp)
target_link_libraries(allegiance-original-dialogs-check PRIVATE allegiance_frontend)
add_executable(allegiance-asset-audit asset_audit.cpp)
target_link_libraries(allegiance-asset-audit PRIVATE allegiance_game)

set(SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
foreach(SHADER scene.vert scene.frag hud.vert hud.frag particle.vert particle.frag gamma.vert gamma.frag)
    add_custom_command(OUTPUT ${SHADER_DIR}/${SHADER}.spv
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR}
        COMMAND Vulkan::glslc --target-env=vulkan1.3 ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER} -o ${SHADER_DIR}/${SHADER}.spv
        DEPENDS shaders/${SHADER})
    list(APPEND SHADER_BINARIES ${SHADER_DIR}/${SHADER}.spv)
endforeach()
add_custom_target(allegiance_shaders DEPENDS ${SHADER_BINARIES})
add_library(allegiance_renderer STATIC renderer.cpp presentation.cpp FramePacer.cpp)
add_executable(allegiance-presentation-check presentation_check.cpp presentation.cpp FramePacer.cpp)
target_compile_features(allegiance-presentation-check PRIVATE cxx_std_20)
target_link_libraries(allegiance-presentation-check PRIVATE Vulkan::Vulkan)
add_test(NAME native-presentation COMMAND allegiance-presentation-check)
target_link_libraries(allegiance_renderer PUBLIC allegiance_assets Vulkan::Vulkan PkgConfig::SDL3)
target_compile_definitions(allegiance_renderer PRIVATE ALLEGIANCE_SHADER_DIR="${SHADER_DIR}")
add_dependencies(allegiance_renderer allegiance_shaders)
add_library(allegiance_input STATIC controller.cpp haptics.cpp input_map.cpp)
target_compile_features(allegiance_input PUBLIC cxx_std_20)
target_link_libraries(allegiance_input PUBLIC PkgConfig::SDL3)
add_executable(allegiance-keyboard-check keyboard_check.cpp settings.cpp)
target_link_libraries(allegiance-keyboard-check PRIVATE allegiance_input)
add_library(allegiance_legacy_settings STATIC legacy_settings.cpp)
target_compile_features(allegiance_legacy_settings PRIVATE cxx_std_17)
target_link_libraries(allegiance_legacy_settings PRIVATE PkgConfig::SDL3)
add_library(allegiance_frontend STATIC frontend.cpp frontend_mapping.cpp frontend_options.cpp frontend_classic.cpp frontend_setup.cpp frontend_research.cpp frontend_results.cpp frontend_games.cpp chat_console.cpp player_pane.cpp game_state_pane.cpp crew_pane.cpp console_pane.cpp)
target_compile_features(allegiance_frontend PUBLIC cxx_std_20)
target_link_libraries(allegiance_frontend PUBLIC allegiance_game allegiance_input allegiance_legacy_settings)
add_executable(allegiance-render-check render_check.cpp)
target_sources(allegiance-render-check PRIVATE hud.cpp settings.cpp original_cursor.cpp)
target_link_libraries(allegiance-render-check PRIVATE allegiance_renderer allegiance_frontend PkgConfig::FONTS)
add_executable(allegiance-ui-hover-check ui_hover_check.cpp)
target_sources(allegiance-ui-hover-check PRIVATE hud.cpp settings.cpp)
target_link_libraries(allegiance-ui-hover-check PRIVATE allegiance_renderer allegiance_frontend PkgConfig::FONTS)
add_executable(allegiance-native WIN32 main.cpp)
target_sources(allegiance-native PRIVATE hud.cpp settings.cpp smoke.cpp original_cursor.cpp)
target_link_libraries(allegiance-native PRIVATE allegiance_renderer allegiance_frontend PkgConfig::FONTS)
if(APPLE)
    target_sources(allegiance_assets PRIVATE system_font_mac.cpp)
    target_link_libraries(allegiance_assets PRIVATE "-framework CoreText" "-framework CoreFoundation")
endif()

add_library(allegiance_training STATIC
    ../training/AbstractTarget.cpp
    ../training/Action.cpp
    ../training/AddRespawnAction.cpp
    ../training/CommandAcknowledgedCondition.cpp
    ../training/Condition.cpp
    ../training/CreateDroneAction.cpp
    ../training/CreateObjectAction.cpp
    ../training/CreateProbeAction.cpp
    ../training/CreateWaypointAction.cpp
    ../training/CurrentTarget.cpp
    ../training/DoDamageAction.cpp
    ../training/DoDamageAdjustableAction.cpp
    ../training/GetAutopilotCondition.cpp
    ../training/GetChatCondition.cpp
    ../training/GetCommandCondition.cpp
    ../training/GetControlActiveCondition.cpp
    ../training/GetKeyCondition.cpp
    ../training/GetPickedCondition.cpp
    ../training/GetProbeCountCondition.cpp
    ../training/GetRadarLODCondition.cpp
    ../training/GetSectorCondition.cpp
    ../training/GetShipHasAmmoCondition.cpp
    ../training/GetShipHasCargoCondition.cpp
    ../training/GetShipHasMountedCondition.cpp
    ../training/GetShipIsDamagedCondition.cpp
    ../training/GetShipIsStoppedCondition.cpp
    ../training/GetStateBitsCondition.cpp
    ../training/GetTargetCondition.cpp
    ../training/GetViewSectorCondition.cpp
    ../training/Goal.cpp
    ../training/KillsReachedCondition.cpp
    ../training/ModifyCreateDroneActionAction.cpp
    ../training/PlayerShipTarget.cpp
    ../training/Predicate.cpp
    ../training/ProxyAction.cpp
    ../training/ProxyCondition.cpp
    ../training/ReleaseConsumerAction.cpp
    ../training/RepairRearmAction.cpp
    ../training/SetAutopilotAction.cpp
    ../training/SetCargoNanAction.cpp
    ../training/SetControlConstraintsAction.cpp
    ../training/SetControlsAction.cpp
    ../training/SetDroneNanAction.cpp
    ../training/SetHUDOverlayAction.cpp
    ../training/SetRadarLODAction.cpp
    ../training/SetSignatureAction.cpp
    ../training/ShipSeesObjectCondition.cpp
    ../training/ShowEyeballAction.cpp
    ../training/ShowPaneAction.cpp
    ../training/SuspendedPlaySoundAction.cpp
    ../training/SuspendedSoundFinishedCondition.cpp
    ../training/SwapWeaponAction.cpp
    ../training/ToggleWeaponAction.cpp
    ../training/Training.cpp
    ../training/TurnShipAboutCondition.cpp
    ../training/TurnToAction.cpp
    ../training/TypeIDTarget.cpp
    ../training/actionlist.cpp
    ../training/andcondition.cpp
    ../training/conditionalaction.cpp
    ../training/conditionlist.cpp
    ../training/endmissionaction.cpp
    ../training/goallist.cpp
    ../training/messageaction.cpp
    ../training/mission10.cpp
    ../training/mission2.cpp
    ../training/mission3.cpp
    ../training/mission4.cpp
    ../training/mission5.cpp
    ../training/mission6.cpp
    ../training/mission8.cpp
    ../training/noaction.cpp
    ../training/ntimescondition.cpp
    ../training/objectmovingtowardscondition.cpp
    ../training/objectpointingatcondition.cpp
    ../training/objectwithinradiuscondition.cpp
    ../training/onetimeaction.cpp
    ../training/orcondition.cpp
    ../training/periodiccondition.cpp
    ../training/playsoundaction.cpp
    ../training/resetaction.cpp
    ../training/resetshipaction.cpp
    ../training/setcommandaction.cpp
    ../training/setdisplaymodeaction.cpp
    ../training/soundfinishedcondition.cpp
    ../training/trainingmission.cpp
    ../training/truecondition.cpp
)
target_link_libraries(allegiance_training PUBLIC allegiance_core)
target_compile_features(allegiance_training PUBLIC cxx_std_20)

add_library(allegiance_game STATIC game.cpp particles.cpp loadouts.cpp endgame_export.cpp)
target_link_libraries(allegiance_game PUBLIC allegiance_core allegiance_assets allegiance_network)
target_compile_features(allegiance_game PUBLIC cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    target_compile_options(allegiance_game PRIVATE
        $<$<CONFIG:Release>:-fwhole-program-vtables>
        $<$<CONFIG:Release>:-fvisibility=hidden>)
endif()
add_executable(allegiance-scale-benchmark scale_benchmark.cpp smoke.cpp)
target_link_libraries(allegiance-scale-benchmark PRIVATE allegiance_game)
add_executable(allegiance-network-scale network_scale.cpp)
target_link_libraries(allegiance-network-scale PRIVATE allegiance_multiplayer)
add_executable(allegiance-network-arena-check network_arena_check.cpp)
target_link_libraries(allegiance-network-arena-check PRIVATE allegiance_network)
add_test(NAME native-network-arenas COMMAND allegiance-network-arena-check)
add_executable(allegiance-network-load network_load.cpp)
target_link_libraries(allegiance-network-load PRIVATE allegiance_multiplayer)
add_executable(allegiance-scale-check scale_check.cpp)
target_link_libraries(allegiance-scale-check PRIVATE allegiance_core allegiance_assets)
add_test(NAME native-scale-policy COMMAND allegiance-scale-check)

add_library(allegiance_audio_pcm STATIC ${NATIVE_AUDIO_DECODER} audio_pcm.cpp)
target_link_libraries(allegiance_audio_pcm PUBLIC allegiance_core allegiance_assets)
if(APPLE)
    target_link_libraries(allegiance_audio_pcm PRIVATE "-framework AVFoundation" "-framework Foundation" "-framework CoreMedia" "-framework AudioToolbox" "-framework CoreFoundation")
endif()
add_library(allegiance_sound_templates STATIC ../soundengine/soundtemplates.cpp ../soundengine/soundutil.cpp ../zlib/event.cpp)
target_link_libraries(allegiance_sound_templates PUBLIC allegiance_audio_pcm)
target_compile_options(allegiance_sound_templates PRIVATE ${NATIVE_FORCE_INCLUDE})
add_library(allegiance_audio_graph STATIC audio_backend.cpp sound_bank.cpp)
target_link_libraries(allegiance_audio_graph PUBLIC allegiance_sound_templates allegiance_assets PkgConfig::OPENAL)
add_library(allegiance_audio STATIC audio.cpp movie.cpp training_slides.cpp)
target_link_libraries(allegiance_audio PUBLIC allegiance_assets allegiance_renderer allegiance_audio_graph PkgConfig::FFMPEG)
target_link_libraries(allegiance-native PRIVATE allegiance_audio)
target_link_libraries(allegiance_game PUBLIC allegiance_training allegiance_audio)
add_library(allegiance_multiplayer STATIC network_session.cpp server_host.cpp)
target_link_libraries(allegiance_multiplayer PUBLIC allegiance_game allegiance_network)
add_executable(allegiance-server server_main.cpp smoke.cpp)
target_link_libraries(allegiance-server PRIVATE allegiance_multiplayer)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(allegiance-server PRIVATE -Wl,--as-needed)
endif()
add_executable(allegiance-network-check network_check.cpp settings.cpp)
target_link_libraries(allegiance-network-check PRIVATE allegiance_multiplayer allegiance_frontend)
add_executable(allegiance-lan-check lan_discovery_check.cpp)
target_link_libraries(allegiance-lan-check PRIVATE allegiance_network)
add_executable(allegiance-directory directory_main.cpp)
target_link_libraries(allegiance-directory PRIVATE allegiance_network)
add_executable(allegiance-directory-check directory_check.cpp)
target_link_libraries(allegiance-directory-check PRIVATE allegiance_network)
add_test(NAME native-directory COMMAND allegiance-directory-check)
target_link_libraries(allegiance-native PRIVATE allegiance_multiplayer)
install(TARGETS allegiance-server allegiance-directory RUNTIME DESTINATION bin)
install(TARGETS allegiance-native RUNTIME DESTINATION bin)
install(FILES ${SHADER_BINARIES} DESTINATION share/freeallegiance/shaders)
if(NOT WIN32)
    install(FILES org.freeallegiance.Native.desktop DESTINATION share/applications)
endif()

add_executable(allegiance-controller-check controller_check.cpp)
target_link_libraries(allegiance-controller-check PRIVATE allegiance_input)
add_test(NAME native-controller COMMAND allegiance-controller-check)
add_executable(allegiance-haptics-check haptics_check.cpp)
target_link_libraries(allegiance-haptics-check PRIVATE allegiance_input)

add_executable(allegiance-world-check world_check.cpp)
target_link_libraries(allegiance-world-check PRIVATE allegiance_game)
add_executable(allegiance-session-check session_check.cpp settings.cpp)
target_link_libraries(allegiance-session-check PRIVATE allegiance_frontend)
add_executable(allegiance-console-check chat_console_check.cpp settings.cpp)
target_link_libraries(allegiance-console-check PRIVATE allegiance_frontend)
add_executable(allegiance-crew-check crew_check.cpp settings.cpp)
target_link_libraries(allegiance-crew-check PRIVATE allegiance_frontend)
add_executable(allegiance-inventory-check inventory_check.cpp)
target_link_libraries(allegiance-inventory-check PRIVATE allegiance_game)
add_executable(allegiance-endgame-check endgame_check.cpp settings.cpp)
target_link_libraries(allegiance-endgame-check PRIVATE allegiance_frontend)
add_executable(allegiance-game-state-check game_state_check.cpp)
target_link_libraries(allegiance-game-state-check PRIVATE allegiance_frontend)
add_executable(allegiance-player-pane-check player_pane_check.cpp settings.cpp)
target_link_libraries(allegiance-player-pane-check PRIVATE allegiance_frontend)
add_executable(allegiance-teleport-check teleport_check.cpp)
target_link_libraries(allegiance-teleport-check PRIVATE allegiance_game)
add_executable(allegiance-flight-feedback-check flight_feedback_check.cpp)
target_link_libraries(allegiance-flight-feedback-check PRIVATE allegiance_game)
add_executable(allegiance-combat-feedback-check combat_feedback_check.cpp)
target_link_libraries(allegiance-combat-feedback-check PRIVATE allegiance_game)
add_executable(allegiance-recovery-check recovery_check.cpp)
target_link_libraries(allegiance-recovery-check PRIVATE allegiance_game)
add_executable(allegiance-allied-credits-check allied_credits_check.cpp)
target_link_libraries(allegiance-allied-credits-check PRIVATE allegiance_game)
add_executable(allegiance-spatial-check spatial_check.cpp)
target_link_libraries(allegiance-spatial-check PRIVATE allegiance_game)
add_executable(allegiance-camera-check camera_check.cpp)
target_link_libraries(allegiance-camera-check PRIVATE allegiance_game)
add_executable(allegiance-actions-check actions_check.cpp)
target_link_libraries(allegiance-actions-check PRIVATE allegiance_core allegiance_assets)
add_executable(allegiance-visibility-check visibility_check.cpp)
target_link_libraries(allegiance-visibility-check PRIVATE allegiance_core allegiance_assets)
add_executable(allegiance-visibility-world-check visibility_world_check.cpp)
target_link_libraries(allegiance-visibility-world-check PRIVATE allegiance_game)
add_executable(allegiance-effects-check effects_check.cpp)
target_link_libraries(allegiance-effects-check PRIVATE allegiance_game)
add_executable(allegiance-lifecycle-check lifecycle_check.cpp)
target_link_libraries(allegiance-lifecycle-check PRIVATE allegiance_game)
add_executable(allegiance-model-lifetime-check model_lifetime_check.cpp)
target_link_libraries(allegiance-model-lifetime-check PRIVATE allegiance_game)
add_executable(allegiance-goals-check goals_check.cpp)
target_link_libraries(allegiance-goals-check PRIVATE allegiance_game)
add_executable(allegiance-carrier-check carrier_check.cpp)
target_link_libraries(allegiance-carrier-check PRIVATE allegiance_game)
add_executable(allegiance-indicator-check indicator_check.cpp)
target_link_libraries(allegiance-indicator-check PRIVATE allegiance_game)
add_executable(allegiance-weapons-check weapons_check.cpp)
target_link_libraries(allegiance-weapons-check PRIVATE allegiance_game)
add_executable(allegiance-targeting-check targeting_check.cpp)
target_link_libraries(allegiance-targeting-check PRIVATE allegiance_game)
add_executable(allegiance-training-check training_check.cpp)
target_link_libraries(allegiance-training-check PRIVATE allegiance_game)
add_executable(allegiance-controls-check controls_check.cpp)
target_link_libraries(allegiance-controls-check PRIVATE allegiance_game)
add_executable(allegiance-commands-check commands_check.cpp smoke.cpp)
target_link_libraries(allegiance-commands-check PRIVATE allegiance_game)
add_executable(allegiance-callbacks-check callbacks_check.cpp)
target_link_libraries(allegiance-callbacks-check PRIVATE allegiance_game)
add_executable(allegiance-simulation-check simulation_check.cpp)
target_link_libraries(allegiance-simulation-check PRIVATE allegiance_game)
add_executable(allegiance-loadouts-check loadouts_check.cpp)
target_link_libraries(allegiance-loadouts-check PRIVATE allegiance_game)
add_executable(allegiance-setup-check setup_check.cpp)
target_link_libraries(allegiance-setup-check PRIVATE allegiance_game)
add_executable(allegiance-players-check players_check.cpp)
target_link_libraries(allegiance-players-check PRIVATE allegiance_game)
add_executable(allegiance-economy-check economy_check.cpp)
target_link_libraries(allegiance-economy-check PRIVATE allegiance_game)
add_executable(allegiance-drone-check drone_check.cpp)
target_link_libraries(allegiance-drone-check PRIVATE allegiance_game)
add_executable(allegiance-audio-check audio_check.cpp)
target_link_libraries(allegiance-audio-check PRIVATE allegiance_audio allegiance_game)
add_executable(allegiance-movie-check movie_check.cpp)
target_link_libraries(allegiance-movie-check PRIVATE allegiance_audio)
add_executable(allegiance-mission-rules-check mission_rules_check.cpp)
target_link_libraries(allegiance-mission-rules-check PRIVATE allegiance_core allegiance_assets)

if(APPLE)
    set(DEFAULT_ARTWORK "$ENV{HOME}/Library/Application Support/Steam/steamapps/common/Allegiance/Artwork")
else()
    set(DEFAULT_ARTWORK "$ENV{HOME}/.local/share/Steam/steamapps/common/Allegiance/Artwork")
endif()
set(ALLEGIANCE_ARTWORK "${DEFAULT_ARTWORK}"
    CACHE PATH "Installed game data for native integration tests")
if(EXISTS "${ALLEGIANCE_ARTWORK}/PCore014.igc")
    add_test(NAME native-original-ui-scripts COMMAND allegiance-ui-script-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-haptics COMMAND allegiance-haptics-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-visibility-world COMMAND allegiance-visibility-world-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-effects COMMAND allegiance-effects-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-audio COMMAND allegiance-audio-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-movie COMMAND allegiance-movie-check "${ALLEGIANCE_ARTWORK}")
    set_tests_properties(native-audio native-movie PROPERTIES ENVIRONMENT "ALSOFT_DRIVERS=null")
    add_test(NAME native-static-core COMMAND allegiance-core-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-training-core COMMAND allegiance-core-check "${ALLEGIANCE_ARTWORK}" PCore014)
    add_test(NAME native-dynamic-records COMMAND allegiance-dynamic-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-core-catalog COMMAND allegiance-core-catalog-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-original-dialogs COMMAND allegiance-original-dialogs-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-asset-inventory COMMAND allegiance-asset-audit "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-models COMMAND allegiance-assets-check "${ALLEGIANCE_ARTWORK}"
        wc_icfig wc_icint wc_icsf fig05 fig21 fig13 ss27 mis05 mis06 mis07 "lens#lens30" "lens#lens60" "lens#lens90" build
        bgrnd03 bgrnd05 bgrnd51 bgrnd52 bgrnd53 bgrnd55 bgrnd56)
    add_test(NAME native-combat COMMAND allegiance-world-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-sessions COMMAND allegiance-session-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-console COMMAND allegiance-console-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-lan-discovery COMMAND allegiance-lan-check)
    add_test(NAME native-network-queued-requests COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case requests)
    set_tests_properties(native-network-queued-requests PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-lifecycle COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case lifecycle)
    set_tests_properties(native-network-lifecycle PROPERTIES TIMEOUT 90)
    add_test(NAME native-network-mission-chat COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case missionchat)
    set_tests_properties(native-network-mission-chat PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-custom-maps COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case maps)
    add_test(NAME native-network-mission-briefings COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case briefing)
    add_test(NAME native-network-allied-rearm COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case allied)
    add_test(NAME native-network-command-focus COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case commandfocus)
    set_tests_properties(native-network-custom-maps native-network-mission-briefings native-network-allied-rearm native-network-command-focus PROPERTIES TIMEOUT 60)
    add_test(NAME native-crew COMMAND allegiance-crew-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-network-crew COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case crews)
    set_tests_properties(native-network-crew PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-docked-crew COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case dockedcrews)
    set_tests_properties(native-network-docked-crew PROPERTIES TIMEOUT 30)
    add_test(NAME native-inventory COMMAND allegiance-inventory-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-endgame COMMAND allegiance-endgame-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-game-state COMMAND allegiance-game-state-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-player-pane COMMAND allegiance-player-pane-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-keyboard COMMAND allegiance-keyboard-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-teleport COMMAND allegiance-teleport-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-flight-feedback COMMAND allegiance-flight-feedback-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-combat-feedback COMMAND allegiance-combat-feedback-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-recovery COMMAND allegiance-recovery-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-allied-credits COMMAND allegiance-allied-credits-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-network-allied-credits COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case credits)
    set_tests_properties(native-network-allied-credits PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-recovery COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case recovery)
    set_tests_properties(native-network-recovery PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-combat-feedback COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case feedback)
    set_tests_properties(native-network-combat-feedback PROPERTIES TIMEOUT 30)
    add_test(NAME native-spatial COMMAND allegiance-spatial-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-camera COMMAND allegiance-camera-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-actions COMMAND allegiance-actions-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-visibility COMMAND allegiance-visibility-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-lifecycle COMMAND allegiance-lifecycle-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-network-health COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case health)
    set_tests_properties(native-network-health PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-throttle COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case throttle)
    set_tests_properties(native-network-throttle PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-weapons COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case weapons)
    set_tests_properties(native-network-weapons PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-teams COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case teams)
    set_tests_properties(native-network-teams PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-alliances COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case alliances)
    set_tests_properties(native-network-alliances PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-randomize COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case randomize)
    set_tests_properties(native-network-randomize PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-core-catalog COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case cores)
    set_tests_properties(native-network-core-catalog PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-create-game COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case create)
    set_tests_properties(native-network-create-game PROPERTIES TIMEOUT 60)
    add_test(NAME native-network-browse COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case browse)
    set_tests_properties(native-network-browse PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-public-directory COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case directory)
    set_tests_properties(native-network-public-directory PROPERTIES TIMEOUT 40)
    add_test(NAME native-network-purchase COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case purchase)
    set_tests_properties(native-network-purchase PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-investments COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case investments)
    set_tests_properties(native-network-investments PROPERTIES TIMEOUT 30)
    add_test(NAME native-network-ship-status COMMAND allegiance-network-check "${ALLEGIANCE_ARTWORK}" --case intel)
    set_tests_properties(native-network-ship-status PROPERTIES TIMEOUT 30)
    add_test(NAME native-model-lifetimes COMMAND allegiance-model-lifetime-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-goals COMMAND allegiance-goals-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-carrier COMMAND allegiance-carrier-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-indicator COMMAND allegiance-indicator-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-weapons COMMAND allegiance-weapons-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-targeting COMMAND allegiance-targeting-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-training COMMAND allegiance-training-check "${ALLEGIANCE_ARTWORK}")
    set_tests_properties(native-training PROPERTIES ENVIRONMENT "ALSOFT_DRIVERS=null")
    add_test(NAME native-controls COMMAND allegiance-controls-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-commands COMMAND allegiance-commands-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-callbacks COMMAND allegiance-callbacks-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-simulation COMMAND allegiance-simulation-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-loadouts COMMAND allegiance-loadouts-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-setup COMMAND allegiance-setup-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-players COMMAND allegiance-players-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-economy COMMAND allegiance-economy-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-drone-completeness COMMAND allegiance-drone-check "${ALLEGIANCE_ARTWORK}")
    add_test(NAME native-mission-rules COMMAND allegiance-mission-rules-check "${ALLEGIANCE_ARTWORK}")
endif()
