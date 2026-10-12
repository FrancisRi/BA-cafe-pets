cmake_minimum_required(VERSION 3.24)
project(ba_cafe_lite_native VERSION 0.2.0 LANGUAGES C CXX RC)
if(NOT WIN32)
    message(FATAL_ERROR "This desktop prototype requires Windows")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
get_filename_component(BA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CMAKE_SKIP_INSTALL_RULES ON)
set(BTCPP_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BTCPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BTCPP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BTCPP_GROOT_INTERFACE OFF CACHE BOOL "" FORCE)
set(BTCPP_SQLITE_LOGGING OFF CACHE BOOL "" FORCE)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
if(EXISTS "${BA_ROOT}/vendor/raylib-6.0/CMakeLists.txt")
    set(raylib_SOURCE_DIR "${BA_ROOT}/vendor/raylib-6.0")
    add_subdirectory("${raylib_SOURCE_DIR}" vendor/raylib EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.zip
        URL_HASH SHA256=a6770924fed7b25de9f6009ec236cd095a99577a3907026da2bfa21a27ad271a)
    FetchContent_MakeAvailable(raylib)
endif()
if(EXISTS "${BA_ROOT}/vendor/BehaviorTree.CPP-4.8.0/CMakeLists.txt")
    set(behaviortree_cpp_SOURCE_DIR "${BA_ROOT}/vendor/BehaviorTree.CPP-4.8.0")
    add_subdirectory("${behaviortree_cpp_SOURCE_DIR}" vendor/behavior EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(behaviortree_cpp
        URL https://github.com/BehaviorTree/BehaviorTree.CPP/archive/refs/tags/4.8.0.zip
        URL_HASH SHA256=af0d212cf3c7fe70a484b92edf263d8d21b70df41909cc83d251f5800bad2205)
    FetchContent_MakeAvailable(behaviortree_cpp)
endif()
set(BA_BINARY_FILENAME "ba_cafe_lite_native.exe")
set(BA_FILE_DESCRIPTION "ba_cafe_lite native desktop pet")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/native_version.rc" @ONLY)
add_executable(ba_cafe_lite_native WIN32 main.cpp gltf_model.cpp native_renderer.cpp "${CMAKE_CURRENT_BINARY_DIR}/native_version.rc")
set(BA_NATIVE_EXECUTABLE_NAME "ba_cafe_lite_native" CACHE STRING "Executable filename (allows building alongside a running version)")
set_target_properties(ba_cafe_lite_native PROPERTIES OUTPUT_NAME "${BA_NATIVE_EXECUTABLE_NAME}" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(ba_cafe_lite_native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${BA_ROOT}/src" "${BA_ROOT}/experimental/three")
target_compile_definitions(ba_cafe_lite_native PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(ba_cafe_lite_native PRIVATE raylib behaviortree_cpp user32 gdi32 comdlg32 shell32 dwmapi comctl32)
if(MSVC)
    target_compile_options(ba_cafe_lite_native PRIVATE /utf-8)
    set_property(TARGET ba_cafe_lite_native PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_link_options(ba_cafe_lite_native PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()
add_custom_command(TARGET ba_cafe_lite_native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ba_cafe_lite_native>/models" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/behavior" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BA_ROOT}/assets/Shiroko.glb" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/models/Shiroko.glb"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/release/defaults.ini" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/defaults.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BA_ROOT}/design/hibiki-cafe-attention.bt.xml" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/behavior/template.bt.xml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BA_ROOT}/experimental/three/design/battle-mode.drawio" "${BA_ROOT}/experimental/three/design/battle-mode.mmd" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/behavior"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${raylib_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/licenses/raylib.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/licenses/GLFW.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/licenses" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${behaviortree_cpp_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/licenses/BehaviorTree.CPP.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/README.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/release/README.txt" "${CMAKE_CURRENT_SOURCE_DIR}/release/CHANGELOG.md" "$<TARGET_FILE_DIR:ba_cafe_lite_native>")
foreach(part IN ITEMS tinyxml2 minitrace minicoro lexy)
    file(GLOB notice "${behaviortree_cpp_SOURCE_DIR}/3rdparty/${part}/LICENSE*")
    list(GET notice 0 notice_file)
    add_custom_command(TARGET ba_cafe_lite_native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${notice_file}" "$<TARGET_FILE_DIR:ba_cafe_lite_native>/licenses/${part}.txt")
endforeach()

set(BA_BINARY_FILENAME "ba_cafe_lite.exe")
set(BA_FILE_DESCRIPTION "ba_cafe_lite offline launcher")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/launcher_version.rc" @ONLY)
add_executable(ba_cafe_lite_release_launcher WIN32 EXCLUDE_FROM_ALL release_launcher.cpp "${CMAKE_CURRENT_BINARY_DIR}/launcher_version.rc")
target_compile_definitions(ba_cafe_lite_release_launcher PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(ba_cafe_lite_release_launcher PRIVATE user32 shell32 comctl32 cabinet bcrypt)
set_target_properties(ba_cafe_lite_release_launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(MSVC)
    target_compile_options(ba_cafe_lite_release_launcher PRIVATE /utf-8)
else()
    target_link_options(ba_cafe_lite_release_launcher PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()
