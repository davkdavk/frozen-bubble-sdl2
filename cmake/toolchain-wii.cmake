# CMake Toolchain file for Wii (devkitPPC)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR powerpc)

set(DEVKITPRO /opt/devkitpro)
set(DEVKITPPC ${DEVKITPRO}/devkitPPC)

set(CMAKE_C_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-gcc)
set(CMAKE_CXX_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-g++)
set(CMAKE_AR ${DEVKITPPC}/bin/powerpc-eabi-ar CACHE FILEPATH "Archive manager")
set(CMAKE_RANLIB ${DEVKITPPC}/bin/powerpc-eabi-ranlib CACHE FILEPATH "Ranlib")

set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO}/portlibs/ppc)
set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO}/libogc ${CMAKE_FIND_ROOT_PATH})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS_INIT "-mhard-float -mcpu=750 -meabi -mno-sdata -DWII")
set(CMAKE_CXX_FLAGS_INIT "-mhard-float -mcpu=750 -meabi -mno-sdata -fno-exceptions -fno-rtti -DWII")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")
