cmake_minimum_required(VERSION 3.16)
project(ColorModelsLab VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_library(color_core STATIC
    src/model/ColorMath.h
    src/model/ColorMath.cpp
    src/viewmodel/ColorViewModel.h
    src/viewmodel/ColorViewModel.cpp
)
target_include_directories(color_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(color_core PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)

add_executable(ColorModelsLab
    src/main.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp
)
target_link_libraries(ColorModelsLab PRIVATE color_core Qt${QT_VERSION_MAJOR}::Widgets)

if(WIN32)
    set_target_properties(ColorModelsLab PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

option(BUILD_TESTS "Собирать юнит-тесты" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

    add_executable(test_colormath tests/test_colormath.cpp)
    target_link_libraries(test_colormath PRIVATE color_core Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)

    add_test(NAME ColorMathTests COMMAND test_colormath)
endif()