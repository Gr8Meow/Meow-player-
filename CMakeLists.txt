cmake_minimum_required(VERSION 3.16)
project(MeowPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MeowPlayer WIN32 src/main.cpp)

if(MSVC)
  target_compile_options(MeowPlayer PRIVATE /W3 /O2 /utf-8)
else()
  target_compile_options(MeowPlayer PRIVATE -O2 -Wall)
  target_link_options(MeowPlayer PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(MeowPlayer PRIVATE user32 gdi32 msimg32 comdlg32 shell32 winhttp)
