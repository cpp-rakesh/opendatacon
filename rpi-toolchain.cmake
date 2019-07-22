# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR armhf)
SET(CMAKE_SYSTEM_VERSION rpi)

# specify the cross compiler
SET(CMAKE_AR "arm-linux-gnueabihf-ar")
SET(CMAKE_ASM_COMPILER "arm-linux-gnueabihf-gcc")
SET(CMAKE_C_COMPILER "arm-linux-gnueabihf-gcc")
SET(CMAKE_CXX_COMPILER "arm-linux-gnueabihf-g++")

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH "$ENV{SYSROOT}")

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY) 

#this isn't detected for some compiler versions
set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)

