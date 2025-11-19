cmake_minimum_required(VERSION 3.12)
project(test)

add_compile_options(
    -g          # 生成 dwarf
    -O0         # 关闭优化
    -fno-omit-frame-pointer  # 保留帧指针，perf/heaptrack 需要
)

add_compile_options(-Wall -Wextra -Werror=return-type)

find_package(Threads REQUIRED)

add_executable(test main.cpp)

target_link_libraries(test PRIVATE Threads::Threads)

target_compile_features(test PRIVATE cxx_std_20)