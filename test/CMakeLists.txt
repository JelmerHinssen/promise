cmake_minimum_required(VERSION 3.14)

include("beforetarget.cmake" OPTIONAL)

file(GLOB PROMISE_TEST
    "*.h"
    "*.cpp"
)
message(${PROMISE_TEST})
add_executable(promise_test ${PROMISE_TEST})
set_target_properties(promise_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;bin")
target_link_libraries(promise_test promise_options promise_lib gtest_main)
target_include_directories(promise_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR}/../src/include)

set_target_properties(promise_test PROPERTIES OUTPUT_NAME promise_test)

add_custom_command(TARGET promise_test POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:promise_test> ${CMAKE_SOURCE_DIR}/bin/)
