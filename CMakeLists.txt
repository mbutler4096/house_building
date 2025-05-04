cmake_minimum_required(VERSION 3.6)
project(house_building)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} Threads::Threads m)
