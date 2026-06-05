cmake_minimum_required(VERSION 3.16)

project(triplayer VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# 设置源文件编码为 UTF-8（MSVC 特有）
if(MSVC)
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /Zc:strictStrings-)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# Qt 自动功能
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 添加头文件目录
include_directories(${PROJECT_SOURCE_DIR}/inc)

# 添加库文件目录
link_directories(${PROJECT_SOURCE_DIR}/lib)

# 查找 Qt6 包
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network Concurrent)

# 3rdpart: media-server libmpeg（纯 C 静态库，放在 find_package 之后避免 AUTOGEN 警告）
add_subdirectory(3rdpart/media-server/libmpeg)

# 项目源文件
set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/yuvplayer.cpp
    src/svacstreamdecoder.cpp
    src/SVACDecoder.cpp
    src/ESDecoder.cpp
    src/PSDecoder.cpp
    src/PSDecoder2.cpp
    src/easylogging++.cc
    src/SDKManager.cpp
    src/tri_svac.cpp
    src/exportdialog.cpp
)

# 项目头文件
set(PROJECT_HEADERS
    inc/mainwindow.h
    inc/yuvplayer.h
    inc/svacstreamdecoder.h
    inc/SVACDecoder.h
    inc/ESDecoder.h
    inc/PSDecoder.h
    inc/PSDecoder2.h
    inc/StreamDecoder.h
    inc/defs.h
    inc/SDKManager.h
    inc/easylogging++.h
    inc/config.h
    inc/tri_core_api.h
    inc/tri_svac.h
    inc/ZxSvacDecLib.h
    inc/base_defs.h
    inc/vitech_invs.h
    inc/exportdialog.h
)

# 添加可执行文件
add_executable(triplayer #WIN32
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

# 链接 Qt6 库
target_link_libraries(triplayer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
    Qt6::Concurrent
)

# 添加第三方库
# 确保这些库文件 (.lib) 对应你的 Qt 和 MSVC 的编译模式（Debug/Release）
target_link_libraries(triplayer PRIVATE
    ZxSvacDecLib
    p35114_sdk
    mpeg
)

# Debug 模式下定义调试宏（用于链接 Qt debug dll）
if(MSVC)
    target_compile_definitions(triplayer PRIVATE
        $<$<CONFIG:Debug>:QT_QTDLL_DEBUG>
    )
endif()
