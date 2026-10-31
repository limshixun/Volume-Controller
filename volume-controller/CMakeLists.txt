cmake_minimum_required(VERSION 3.10)
project(volume_controller)

# Find the library
find_library(SERIALPORT_LIB serialport REQUIRED)

# Compiles volume controller into executable
add_executable(volume_controller volume_controller.c)

# Link the libraries to executable
target_link_libraries(volume_controller PRIVATE ${SERIALPORT_LIB})

