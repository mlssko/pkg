cmake_minimum_required(VERSION 3.16)

project(ImageLab3 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Без оптимизации циклы по пикселям работают очень медленно,
# поэтому по умолчанию собираем Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent)

add_executable(ImageLab3
    main.cpp
    mainwindow.h
    mainwindow.cpp
    widgets.h
    widgets.cpp
    imageconvert.h
    imageconvert.cpp
    pixelbuffer.h
    pixelbuffer.cpp
    algorithms.h
    algorithms.cpp
    parallel.h
)

target_link_libraries(ImageLab3 PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
)

if(WIN32)
    set_target_properties(ImageLab3 PROPERTIES WIN32_EXECUTABLE ON)
endif()
