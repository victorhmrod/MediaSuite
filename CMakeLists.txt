cmake_minimum_required(VERSION 3.24)
project(MediaSuite VERSION 1.2.0 LANGUAGES CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Sql Svg OpenGLWidgets Concurrent Network Test)
qt_standard_project_setup()

set(LOCALFLIX_LIBMPV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmpv" CACHE PATH "libmpv headers and DLL (scripts/fetch-suite-deps.ps1)")
set(LOCALFLIX_MPV_DLL "${LOCALFLIX_LIBMPV_DIR}/libmpv-2.dll")
include(suite/vendor/localflix/cmake/LibmpvImportLib.cmake)
localflix_generate_mpv_import_lib("${LOCALFLIX_MPV_DLL}" "${CMAKE_BINARY_DIR}/mpv-implib" LOCALFLIX_MPV_IMPLIB)
add_library(mpv::libmpv SHARED IMPORTED GLOBAL)
set_target_properties(mpv::libmpv PROPERTIES IMPORTED_LOCATION "${LOCALFLIX_MPV_DLL}" IMPORTED_IMPLIB "${LOCALFLIX_MPV_IMPLIB}" INTERFACE_INCLUDE_DIRECTORIES "${LOCALFLIX_LIBMPV_DIR}/include")

function(localflix_set_warnings target)
    target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive- /Zc:__cplusplus /Zc:preprocessor)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endfunction()
add_subdirectory(suite/vendor/localflix/src/core)
add_subdirectory(suite/vendor/localflix/src/mpv)
add_subdirectory(suite/vendor/localflix/src/ui)

file(GLOB_RECURSE MAGNIFY_SOURCES CONFIGURE_DEPENDS
    suite/vendor/magnify/src/core/*.cpp suite/vendor/magnify/src/core/*.h
    suite/vendor/magnify/src/engines/*.cpp suite/vendor/magnify/src/engines/*.h
    suite/vendor/magnify/src/hardware/*.cpp suite/vendor/magnify/src/hardware/*.h
    suite/vendor/magnify/src/presets/*.cpp suite/vendor/magnify/src/presets/*.h
    suite/vendor/magnify/src/watch/*.cpp suite/vendor/magnify/src/watch/*.h
    suite/vendor/magnify/src/ui/*.cpp suite/vendor/magnify/src/ui/*.h)
add_library(magnify STATIC ${MAGNIFY_SOURCES}
    suite/vendor/magnify/src/plugins/PluginManager.cpp suite/vendor/magnify/src/plugins/PluginManager.h)
target_include_directories(magnify PUBLIC suite/vendor/magnify/src)
target_link_libraries(magnify PUBLIC Qt6::Widgets Qt6::Concurrent Qt6::Network)
localflix_set_warnings(magnify)
target_link_libraries(localflix_ui PUBLIC magnify Qt6::Network)
target_include_directories(localflix_ui PUBLIC suite/src)
target_sources(localflix_ui PRIVATE suite/src/DownloadsView.cpp suite/src/DownloadsView.h)
target_compile_definitions(localflix_ui PRIVATE MEDIA_SUITE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

qt_add_executable(MediaSuite WIN32 suite/src/main.cpp
    suite/vendor/localflix/resources/resources.qrc suite/vendor/magnify/resources/icon.qrc)
target_link_libraries(MediaSuite PRIVATE localflix_ui)
target_compile_definitions(MediaSuite PRIVATE MEDIA_SUITE_VERSION="${PROJECT_VERSION}")
localflix_set_warnings(MediaSuite)
add_custom_command(TARGET MediaSuite POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LOCALFLIX_MPV_DLL}" "$<TARGET_FILE_DIR:MediaSuite>"
    VERBATIM)

option(MEDIA_SUITE_BUILD_TESTS "Build library regression tests" ON)
set(MEDIA_SUITE_TESTDATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../LocalFlix/testdata/videos" CACHE PATH "Synthetic clips for playback regression tests")
if(MEDIA_SUITE_BUILD_TESTS)
    enable_testing()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-logs")
    set(LOCALFLIX_BUILD_GUI_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(suite/vendor/localflix/tests)
    add_executable(tst_suite suite/tests/tst_suite.cpp suite/vendor/localflix/resources/resources.qrc)
    target_link_libraries(tst_suite PRIVATE localflix_ui Qt6::Test)
    localflix_set_warnings(tst_suite)
    add_test(NAME tst_suite COMMAND tst_suite -o -,txt -o ${CMAKE_BINARY_DIR}/test-logs/tst_suite.txt,txt)
    set_tests_properties(tst_suite PROPERTIES TIMEOUT 90 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_QPA_PLATFORM=set:offscreen")
endif()
