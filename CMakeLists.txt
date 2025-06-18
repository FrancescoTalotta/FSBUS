cmake_minimum_required(VERSION 3.0.0)
project(fsbus VERSION 3.5.0)

set(HEADER_FILES dudp.h dwstring.h fsbus.h fsbusdll.h)
set(CPP_FILES crypt.cpp dfsbus.cpp dfs.cpp dsystem.cpp dtimer.cpp dudp.cpp dwstring.cpp dxsound.cpp fsbus.cpp maindll.cpp StdAfx.cpp)


add_library(fsbus SHARED ${CPP_FILES} ${HEADER_FILES})
target_link_libraries(fsbus dsound)

target_link_libraries(fsbus winmm.lib)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# Add post-build commands to copy the files 
add_custom_command( 
    TARGET fsbus 
    POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy 
            $<TARGET_FILE:fsbus>                # Copy the DLL 
            C:/Users/talotta/Documents/PROGRAMS/GA_SIM/build/Release 
 
    COMMAND ${CMAKE_COMMAND} -E copy 
            ${CMAKE_SOURCE_DIR}/fsbus.h 
            C:/Users/talotta/Documents/PROGRAMS/GA_SIM 
 
    COMMAND ${CMAKE_COMMAND} -E copy 
            ${CMAKE_BINARY_DIR}/Release/fsbus.lib 
            C:/Users/talotta/Documents/PROGRAMS/GA_SIM 
)