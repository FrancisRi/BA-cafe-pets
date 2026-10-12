cmake_minimum_required(VERSION 3.24)
project(ba_cafe_lite VERSION 0.1.2 LANGUAGES C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "This Demo requires Windows.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(PLATFORM Desktop CACHE STRING "" FORCE)
include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/raylib-6.0/CMakeLists.txt")
    add_subdirectory(vendor/raylib-6.0 EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.zip
        URL_HASH SHA256=a6770924fed7b25de9f6009ec236cd095a99577a3907026da2bfa21a27ad271a)
    FetchContent_MakeAvailable(raylib)
endif()
set(BTCPP_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# This standalone demo has no CMake install/export package. BT.CPP's static
# vendored helper targets are intentionally linked only into our executables.
set(CMAKE_SKIP_INSTALL_RULES ON)
set(BTCPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BTCPP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BTCPP_GROOT_INTERFACE OFF CACHE BOOL "" FORCE)
set(BTCPP_SQLITE_LOGGING OFF CACHE BOOL "" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/BehaviorTree.CPP-4.8.0/CMakeLists.txt")
    add_subdirectory(vendor/BehaviorTree.CPP-4.8.0 EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(behaviortree_cpp
        URL https://github.com/BehaviorTree/BehaviorTree.CPP/archive/refs/tags/4.8.0.zip
        URL_HASH SHA256=af0d212cf3c7fe70a484b92edf263d8d21b70df41909cc83d251f5800bad2205)
    FetchContent_MakeAvailable(behaviortree_cpp)
endif()
add_executable(desktop_pet WIN32 src/main.cpp)
target_link_libraries(desktop_pet PRIVATE raylib behaviortree_cpp user32 gdi32 comdlg32)
target_compile_definitions(desktop_pet PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(desktop_pet PRIVATE /W4 /utf-8)
else()
    target_compile_options(desktop_pet PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(desktop_pet PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_custom_target(copy_pet_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:desktop_pet>/assets"
    DEPENDS desktop_pet)
# The editable XML lives in design/. Do not overwrite a user's runtime edits on rebuild.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/behavior")
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/behavior/hibiki.bt.xml")
    configure_file(design/hibiki-cafe-attention.bt.xml behavior/hibiki.bt.xml COPYONLY)
endif()
add_executable(behavior_tests tests/behavior_tests.cpp)
target_include_directories(behavior_tests PRIVATE src)
target_link_libraries(behavior_tests PRIVATE behaviortree_cpp)
if(MINGW)
    target_link_options(behavior_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
enable_testing()
add_test(NAME pet_behavior COMMAND behavior_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/default.bt.xml" "${CMAKE_CURRENT_SOURCE_DIR}/design/hibiki-cafe-attention.bt.xml")
add_executable(ground_tests tests/ground_tests.cpp)
target_include_directories(ground_tests PRIVATE src)
target_link_libraries(ground_tests PRIVATE raylib)
if(MINGW)
    target_link_options(ground_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME pet_ground COMMAND ground_tests)
add_executable(battle_tests tests/battle_tests.cpp)
target_include_directories(battle_tests PRIVATE src)
target_link_libraries(battle_tests PRIVATE behaviortree_cpp raylib)
if(MINGW)
    target_link_options(battle_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME pet_battle COMMAND battle_tests)
add_executable(transition_tests tests/transition_tests.cpp)
target_include_directories(transition_tests PRIVATE src)
target_link_libraries(transition_tests PRIVATE raylib)
if(MINGW)
    target_link_options(transition_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME pet_animation_transition COMMAND transition_tests)
add_executable(gltf_parent_tests tests/gltf_parent_tests.cpp)
target_link_libraries(gltf_parent_tests PRIVATE raylib)
if(MINGW)
    target_link_options(gltf_parent_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME gltf_animated_parent COMMAND gltf_parent_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/animated_parent.gltf")

option(BUILD_THREE_PET "Build the separate Three.js/WebView2 prototype" OFF)
if(BUILD_THREE_PET)
    enable_language(RC)
    configure_file(experimental/three/app_version.rc.in "${CMAKE_BINARY_DIR}/ba_cafe_lite_version.rc" @ONLY)
    add_executable(three_desktop_tests tests/three_desktop_tests.cpp)
    target_include_directories(three_desktop_tests PRIVATE src experimental/three)
    target_link_libraries(three_desktop_tests PRIVATE raylib behaviortree_cpp user32 gdi32 dwmapi)
    target_compile_definitions(three_desktop_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    if(MINGW)
        target_link_options(three_desktop_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME three_desktop COMMAND three_desktop_tests)
    set(WEBVIEW2_SDK "${CMAKE_CURRENT_SOURCE_DIR}/vendor/webview2-1.0.3650.58")
    if(NOT EXISTS "${WEBVIEW2_SDK}/build/native/include/WebView2.h")
        message(FATAL_ERROR "Run tools/setup_three_pet.ps1 first")
    endif()
    foreach(pet_three_target IN ITEMS desktop_pet_three desktop_pet_three_smooth)
    set(pet_three_directory "three")
    if(pet_three_target STREQUAL "desktop_pet_three_smooth")
        set(pet_three_directory "three-smooth")
    endif()
    add_executable(${pet_three_target} WIN32 experimental/three/main.cpp)
    target_sources(${pet_three_target} PRIVATE "${CMAKE_BINARY_DIR}/ba_cafe_lite_version.rc")
    target_include_directories(${pet_three_target} PRIVATE src "${WEBVIEW2_SDK}/build/native/include")
    target_link_libraries(${pet_three_target} PRIVATE raylib behaviortree_cpp user32 gdi32 comdlg32 ole32 shlwapi dcomp dwmapi)
    target_compile_definitions(${pet_three_target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    set_target_properties(${pet_three_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${pet_three_directory}")
    set_property(TARGET ${pet_three_target} APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/index.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/pet.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/outline.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/silhouette.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/motion.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/animation_catalog.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/animation_groups.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/animation_blend.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/foot_stops.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/spatial_bounds.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/settings.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/settings.css"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/settings.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app/alpha_mask.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/design/battle-mode.drawio"
        "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/design/battle-mode.mmd")
    if(MINGW)
        target_link_options(${pet_three_target} PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
    add_custom_command(TARGET ${pet_three_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${WEBVIEW2_SDK}/build/native/x64/WebView2Loader.dll" "$<TARGET_FILE_DIR:${pet_three_target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/app" "$<TARGET_FILE_DIR:${pet_three_target}>/app"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${pet_three_target}>/app/three/build" "$<TARGET_FILE_DIR:${pet_three_target}>/app/three/examples/jsm/loaders" "$<TARGET_FILE_DIR:${pet_three_target}>/app/three/examples/jsm/utils"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/three-reference/package/build/three.module.js" "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/three-reference/package/build/three.core.js" "$<TARGET_FILE_DIR:${pet_three_target}>/app/three/build"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/three-reference/package/examples/jsm/loaders/GLTFLoader.js" "$<TARGET_FILE_DIR:${pet_three_target}>/app/three/examples/jsm/loaders"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/three-reference/package/examples/jsm/utils/BufferGeometryUtils.js" "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/three-reference/package/examples/jsm/utils/SkeletonUtils.js" "$<TARGET_FILE_DIR:${pet_three_target}>/app/three/examples/jsm/utils"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/three-reference/package/LICENSE" "$<TARGET_FILE_DIR:${pet_three_target}>/app/three/LICENSE"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${pet_three_target}>/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy "${WEBVIEW2_SDK}/LICENSE.txt" "$<TARGET_FILE_DIR:${pet_three_target}>/licenses/WebView2-sdk.txt"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/vendor/BehaviorTree.CPP-4.8.0/LICENSE" "$<TARGET_FILE_DIR:${pet_three_target}>/licenses/BehaviorTree.CPP.txt"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/vendor/raylib-6.0/LICENSE" "$<TARGET_FILE_DIR:${pet_three_target}>/licenses/raylib.txt"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${pet_three_target}>/models" "$<TARGET_FILE_DIR:${pet_three_target}>/behavior"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/Hibiki.glb" "$<TARGET_FILE_DIR:${pet_three_target}>/models/Hibiki.glb"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/Shiroko.glb" "$<TARGET_FILE_DIR:${pet_three_target}>/models/Shiroko.glb"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/design/hibiki-cafe-attention.bt.xml" "$<TARGET_FILE_DIR:${pet_three_target}>/behavior/template.bt.xml")
    add_custom_command(TARGET ${pet_three_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/design/battle-mode.drawio" "${CMAKE_CURRENT_SOURCE_DIR}/experimental/three/design/battle-mode.mmd" "$<TARGET_FILE_DIR:${pet_three_target}>/behavior")
    endforeach()
    add_executable(three_release_launcher WIN32 EXCLUDE_FROM_ALL experimental/three/release_launcher.cpp)
    target_sources(three_release_launcher PRIVATE "${CMAKE_BINARY_DIR}/ba_cafe_lite_version.rc")
    target_compile_definitions(three_release_launcher PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_link_libraries(three_release_launcher PRIVATE cabinet bcrypt user32 shell32 advapi32 comctl32)
    if(MINGW)
        target_link_options(three_release_launcher PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
    if(MSVC)
        target_compile_options(three_release_launcher PRIVATE /utf-8)
        set_property(TARGET three_release_launcher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
        add_executable(three_scene_native WIN32 EXCLUDE_FROM_ALL tests/three_scene_native.cpp)
        target_include_directories(three_scene_native PRIVATE src "${WEBVIEW2_SDK}/build/native/include")
        target_link_libraries(three_scene_native PRIVATE raylib behaviortree_cpp user32 gdi32 comdlg32 ole32 shlwapi dcomp dwmapi)
        target_compile_definitions(three_scene_native PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
        if(MINGW)
            target_link_options(three_scene_native PRIVATE -municode -static -static-libgcc -static-libstdc++)
        endif()
endif()
