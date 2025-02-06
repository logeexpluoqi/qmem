# 设置 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(qmem)

# 设置 C 标准为 C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# 指定使用 GCC 作为 C 和 C++ 编译器
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

# 添加可执行文件
add_executable(qmem main.c qmem.c)

# 如果有库文件，可以添加库
# add_library(my_library STATIC my_library.c)

# 如果需要链接库，可以使用 target_link_libraries
# target_link_libraries(qmem my_library)