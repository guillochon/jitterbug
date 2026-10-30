# Jitterbug - a system-wide entropy source for Mac OS 9.
#
# Host build (simulator, tools, tests):
#     cmake -S . -B build-host && cmake --build build-host && ctest --test-dir build-host
# Mac build (Retro68 PowerPC):
#     scripts/build-mac.sh      (passes the Retro68 toolchain file)
cmake_minimum_required(VERSION 3.10)
project(Jitterbug C)

# Retro68's toolchain files define add_application(); that is our signal.
if(COMMAND add_application OR CMAKE_SYSTEM_NAME MATCHES "^Retro")
    set(JB_MAC TRUE)
else()
    set(JB_MAC FALSE)
endif()

# Mac builds default to -Os: the unoptimized core costs ~2.6x more CPU per
# sample on a G3, and code lives in a small system heap.
if(JB_MAC AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()

# Portable core: strict C89 so it also builds under CodeWarrior/MPW if ever needed.
add_library(jbcore STATIC
    core/jb_sha256.c core/jb_drbg.c core/jb_ring.c core/jb_health.c
    core/jb_pool.c core/jb_engine.c core/jb_policy.c core/jb_trace.c)
target_include_directories(jbcore PUBLIC include)
set_target_properties(jbcore PROPERTIES C_STANDARD 90 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(NOT JB_MAC)
    target_compile_options(jbcore PRIVATE -Wall -Wextra -pedantic -Werror)
    target_link_libraries(jbcore PUBLIC m)
endif()

if(JB_MAC)
    add_subdirectory(mac)
else()
    # The simulator must never end up in a Mac build.
    add_library(jbsim STATIC sim/jb_sim.c)
    target_link_libraries(jbsim PUBLIC jbcore m)
    set_target_properties(jbsim PROPERTIES C_STANDARD 99)
    target_compile_options(jbsim PRIVATE -Wall -Wextra)

    add_executable(jb_sim tools/jb_sim_main.c)
    target_link_libraries(jb_sim jbsim)
    set_target_properties(jb_sim PROPERTIES C_STANDARD 99)

    enable_testing()
    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core jbcore)
    add_executable(test_sim tests/test_sim.c)
    target_link_libraries(test_sim jbsim)
    add_test(NAME core COMMAND test_core)
    add_test(NAME sim  COMMAND test_sim)
endif()
