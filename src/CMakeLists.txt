cmake_minimum_required(VERSION 3.14)

include("beforetarget.cmake" OPTIONAL)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.cpp " ")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/nullmain.cpp " ")
file(GLOB_RECURSE PROMISE_SRC
    "*.h"
    "*.cpp"
)
list(APPEND PROMISE_SRC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
list(REMOVE_ITEM PROMISE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_library(promise_options INTERFACE)

if(MSVC)
target_compile_options(promise_options INTERFACE /W4 /Zc:preprocessor)
target_compile_definitions(promise_options INTERFACE DEBUG_MODE=$<CONFIG:Debug>)
target_link_options(promise_options INTERFACE /ignore:4099)
else()
target_compile_options(promise_options INTERFACE -Wall -Wextra -Wpedantic -Werror)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
target_compile_options(promise_options INTERFACE -g)
target_compile_definitions(promise_options INTERFACE DEBUG_MODE=1)
else()
target_compile_options(promise_options INTERFACE -O2)
target_compile_definitions(promise_options INTERFACE DEBUG_MODE=0)
endif()
endif()

add_library(promise_lib STATIC ${PROMISE_SRC})
add_executable(promise_run "main.cpp" ${CMAKE_CURRENT_BINARY_DIR}/nullmain.cpp)
set_target_properties(promise_run PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;bin")
target_include_directories(promise_lib PRIVATE . PUBLIC ./include)
target_link_libraries(promise_lib promise_options ${all_libraries})
target_link_libraries(promise_run promise_lib promise_options)
 add_custom_command(TARGET promise_lib
     POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${CMAKE_CURRENT_BINARY_DIR}/nullmain.cpp)

set_target_properties(promise_run PROPERTIES OUTPUT_NAME promise)
set_target_properties(promise_lib PROPERTIES OUTPUT_NAME promise)

add_custom_command(TARGET promise_run POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:promise_run> ${CMAKE_SOURCE_DIR}/bin/)
