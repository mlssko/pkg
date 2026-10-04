cmake_minimum_required(VERSION 3.16)
project(ImageInfoReader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

qt_add_executable(ImageInfoReader
    main.cpp
    MainWindow.h
    MainWindow.cpp
    ImageParser.h
    ImageParser.cpp
)

target_link_libraries(ImageInfoReader PRIVATE Qt6::Widgets Qt6::Concurrent)
set_target_properties(ImageInfoReader PROPERTIES WIN32_EXECUTABLE TRUE)
