cmake_minimum_required(VERSION 3.22)
project(FPSNametag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(FPSNametag SHARED src/main.cpp)

target_include_directories(FPSNametag PRIVATE include/)

target_link_libraries(FPSNametag android log)

target_compile_options(FPSNametag PRIVATE
    -O2
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)

target_link_options(FPSNametag PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,--allow-shlib-undefined
    -Wl,--unresolved-symbols=ignore-all
)
