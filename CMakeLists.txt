cmake_minimum_required(VERSION 3.22)
project(FPSTagMod)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(FPSTagMod SHARED src/main.cpp)

target_include_directories(FPSTagMod PRIVATE include/)

target_link_libraries(FPSTagMod android log)

target_compile_options(FPSTagMod PRIVATE
    -O2
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)

target_link_options(FPSTagMod PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,--allow-shlib-undefined
    -Wl,--unresolved-symbols=ignore-all
)
