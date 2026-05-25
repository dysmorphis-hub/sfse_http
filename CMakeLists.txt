cmake_minimum_required(VERSION 3.21)
project(sfse_http VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match vcpkg's static-CRT triplet (x64-windows-static).
# Without this, MSVC defaults to /MD (dynamic CRT) and cpr.lib (built /MT) won't link.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Windows defines we want everywhere
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

# ── vcpkg-installed dependencies ──
find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)  # needed by CommonLibSF REX::LOG

# ── CommonLibSF (Papyrus VM access, RE classes) ──
set(COMMONLIBSF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSF" CACHE PATH "Path to CommonLibSF clone")
set(COMMONLIB_SHARED_ROOT "${COMMONLIBSF_ROOT}/lib/commonlib-shared")

if(NOT EXISTS "${COMMONLIBSF_ROOT}/include/RE/B/BSScriptUtil.h")
    message(FATAL_ERROR
        "CommonLibSF not found at ${COMMONLIBSF_ROOT}.\n"
        "Clone with submodules:\n"
        "  git clone --recurse-submodules https://github.com/libxse/CommonLibSF extern/CommonLibSF")
endif()

if(NOT EXISTS "${COMMONLIB_SHARED_ROOT}/include/REX/REX.h")
    message(FATAL_ERROR
        "CommonLibSF's commonlib-shared submodule not initialized.\n"
        "Run:\n"
        "  cd extern/CommonLibSF && git submodule update --init --recursive")
endif()

# Collect CommonLibSF source files - we build them into our DLL since the
# library doesn't ship a CMake target.
file(GLOB_RECURSE COMMONLIBSF_SOURCES CONFIGURE_DEPENDS
    "${COMMONLIBSF_ROOT}/src/*.cpp"
    "${COMMONLIB_SHARED_ROOT}/src/*.cpp"
)

# Exclude files we don't need - they pull in spdlog/wide-string APIs that
# don't match the vcpkg spdlog version. We don't use REX::LOG or SFSE::Init;
# our plugin has its own logging path (none) and uses raw SFSE plugin entry.
list(REMOVE_ITEM COMMONLIBSF_SOURCES
    "${COMMONLIB_SHARED_ROOT}/src/REX/LOG.cpp"
    "${COMMONLIBSF_ROOT}/src/SFSE/API.cpp"
)

# ── Eve plugin source files ──
set(EVE_SOURCES
    library.cpp
    src/dictionary.cpp
    src/http_client.cpp
    src/reply_queue.cpp
    src/papyrus_api.cpp
    src/stubs.cpp
)

# ── Build the plugin ──
add_library(sfse_http SHARED ${EVE_SOURCES} ${COMMONLIBSF_SOURCES})

target_include_directories(sfse_http PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../sfse/sfse
    ${CMAKE_CURRENT_SOURCE_DIR}/../sfse/sfse_common
    ${COMMONLIBSF_ROOT}/include
    ${COMMONLIB_SHARED_ROOT}/include
)

target_link_libraries(sfse_http PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
    # System libs referenced by CommonLibSF's REX::W32 wrappers
    d3d11
    d3dcompiler
    dxgi
    version
    dbghelp
)

# CommonLibSF source files assume SFSE/Impl/PCH.h is precompiled and force-included
# (xmake handles this automatically via set_pcxxheader). Replicate that in CMake.
# Without this, every CommonLibSF .cpp fails on std::size_t / std::formatter / etc.
target_precompile_headers(sfse_http PRIVATE
    "${COMMONLIBSF_ROOT}/include/SFSE/Impl/PCH.h"
)

# ── Output configuration ──
set_target_properties(sfse_http PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/SFSE/Plugins"
    PREFIX ""
)

# ── Optional: post-build copy to Starfield install ──
# set(STARFIELD_INSTALL "F:/SteamLibrary/steamapps/common/Starfield" CACHE PATH "Starfield install dir for auto-install")
# if(STARFIELD_INSTALL AND EXISTS "${STARFIELD_INSTALL}")
#     add_custom_command(TARGET sfse_http POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#             "$<TARGET_FILE:sfse_http>"
#             "${STARFIELD_INSTALL}/Data/SFSE/Plugins/$<TARGET_FILE_NAME:sfse_http>"
#         COMMENT "Auto-installing sfse_http.dll to Starfield")
# endif()
