cmake_minimum_required(VERSION 3.1)
project(tsc_clock C CXX)

#set(CMAKE_CXX_STANDARD 03)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#check_cxx_source_runs("
#    int main()
#    {
#        #if !defined(__i386__) && \
#            !defined(__x86_64__) && \
#            !defined(_M_IX86) && \
#            !defined(_M_X64)
#            Compile error: not x86 CPU architecture
#        #endif
#        return 0;
#    }"
#    cpu_x86)

#if (cpu_x86)
if (1)
    #cmake_push_check_state()
    #set(CMAKE_REQUIRED_FLAGS -Werror)
    #check_cxx_compiler_flag(-march=native march_native)
    #cmake_pop_check_state()

    if(march_native)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif()
endif()

add_library(tsc_clock STATIC ${CMAKE_CURRENT_SOURCE_DIR}/tsc_clock.cpp)
target_compile_options(tsc_clock PRIVATE "-O3")

add_executable(test_tsc_clock test/test_tsc_clock.cpp)
target_include_directories(test_tsc_clock PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_tsc_clock tsc_clock)

enable_testing()
add_test(test_tsc_clock test_tsc_clock)

install(FILES tsc_clock.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES tsc_clock.a DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)


