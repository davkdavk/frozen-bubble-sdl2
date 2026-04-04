cmake_minimum_required(VERSION 3.13)

project(frozen-bubble-wii)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR powerpc)

set(DEVKITPRO /opt/devkitpro)
set(DEVKITPPC ${DEVKITPRO}/devkitPPC)

set(CMAKE_C_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-gcc)
set(CMAKE_CXX_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-g++)
set(CMAKE_AR ${DEVKITPPC}/bin/powerpc-eabi-ar CACHE FILEPATH "Archive manager")
set(CMAKE_RANLIB ${DEVKITPPC}/bin/powerpc-eabi-ranlib CACHE FILEPATH "Ranlib")
set(CMAKE_LINKER ${DEVKITPPC}/bin/powerpc-eabi-g++)
set(CMAKE_CMAKE_CURRENT_LIST_DIR /home/davey/frozenbubble/frozen-bubble-sdl2/cmake)

set(OGC_INCLUDE ${DEVKITPRO}/libogc/include)
set(OGC_LIB ${DEVKITPRO}/libogc/lib/wii)

include_directories(${OGC_INCLUDE})
include_directories(${CMAKE_SOURCE_DIR}/inc)
include_directories(${CMAKE_SOURCE_DIR}/inc/SDL2)

add_compile_options(-g -O2 -Wall -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -DWII -D__wii__ -fno-rtti -fexceptions -I${CMAKE_SOURCE_DIR}/inc)

set(WII_SOURCES
    src/sdl_ogc_impl.cpp
    src/wii_input.cpp
    src/main_wii.cpp
    src/iniparser_stub.cpp
    src/frozenbubble.cpp
    src/menubutton.cpp
    src/mainmenu.cpp
    src/gamesettings.cpp
    src/audiomixer.cpp
    src/shaderstuff.cpp
    src/bubblegame.cpp
    src/transitionmanager.cpp
    src/ttftext.cpp
    src/highscoremanager.cpp
)

add_executable(frozen-bubble-wii.elf ${WII_SOURCES})

set_property(TARGET frozen-bubble-wii.elf PROPERTY CXX_STANDARD 17)
set_property(TARGET frozen-bubble-wii.elf PROPERTY CXX_EXTENSIONS OFF)

target_compile_definitions(frozen-bubble-wii.elf PRIVATE DATA_DIR=\"sd:/apps/frozenbubble/share\")

target_link_libraries(frozen-bubble-wii.elf PRIVATE
    -lwiiuse -lbte -lfat -logc -lm -lc -lstdc++ -lgcc
)

set(CMAKE_EXE_LINKER_FLAGS "-g -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/frozen-bubble-wii.elf.map -Wl,--gc-sections -L${OGC_LIB} -Wl,-e,__app_start -fno-exceptions -T${DEVKITPPC}/powerpc-eabi/lib/rvl.ld -Wl,--defsym,__isIPL=0")

add_custom_command(OUTPUT frozen-bubble-wii.dol
    COMMAND python3 ${CMAKE_SOURCE_DIR}/cmake/elf2dol.py frozen-bubble-wii.elf frozen-bubble-wii.dol
    DEPENDS frozen-bubble-wii.elf
    COMMENT "Creating Wii DOL file"
)

add_custom_target(dol ALL DEPENDS frozen-bubble-wii.dol)
