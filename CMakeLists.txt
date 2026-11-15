cmake_minimum_required(VERSION 3.16)
project(shpunt_limit_state LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(shpunt_limit_state
    main.cpp
    calcengine.cpp
    mainwindow.cpp
    calcengine.h
    mainwindow.h
)

target_link_libraries(shpunt_limit_state PRIVATE Qt6::Widgets)

# Debug — консольная подсистема (виден терминал),
# Release и остальные конфигурации — GUI-подсистема (без терминала)
set_target_properties(shpunt_limit_state PROPERTIES
    WIN32_EXECUTABLE $<IF:$<CONFIG:Debug>,FALSE,TRUE>
)
