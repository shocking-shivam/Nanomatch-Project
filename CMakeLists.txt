cmake_minimum_required(VERSION 3.29.0)

project(LimitOrderBook VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Profile, Debug)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release;Profile;Debug")

# Profile mirrors Release compile opts but keeps frame pointers + debug info (no strip).
set(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer -g"
    CACHE STRING "Flags used by the C++ compiler for Profile builds." FORCE)

enable_testing()

add_subdirectory(googletest)

set(Headers
    ./Limit_Order_Book/Book.hpp
    ./Limit_Order_Book/Limit.hpp
    ./Limit_Order_Book/Order.hpp
    ./Process_Orders/OrderPipeline.hpp
    ./Generate_Orders/GenerateOrders.hpp
)
set(Sources
    ./Limit_Order_Book/Book.cpp
    ./Limit_Order_Book/Limit.cpp
    ./Limit_Order_Book/Order.cpp
    ./Process_Orders/OrderPipeline.cpp
    ./Generate_Orders/GenerateOrders.cpp
)

# Lean binary flags for the matching engine (Release / Profile only).
set(ENGINE_LEAN_COMPILE_FLAGS
    -O3
    -march=native
    -fno-exceptions
    -fno-rtti
    -funroll-loops
    -DNDEBUG
)

function(apply_engine_lean_flags target)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "Profile")
        target_compile_options(${target} PRIVATE ${ENGINE_LEAN_COMPILE_FLAGS})
        if(CMAKE_BUILD_TYPE STREQUAL "Profile")
            target_compile_options(${target} PRIVATE -fno-omit-frame-pointer -g)
        endif()
    endif()
endfunction()

function(apply_engine_lean_link target)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(${target} PRIVATE -s)
    endif()
endfunction()

# Optimized engine (slab pool + flat arrays)
add_library(${PROJECT_NAME}_lib STATIC ${Sources} ${Headers})
apply_engine_lean_flags(${PROJECT_NAME}_lib)

# Define the executable target
add_executable(${PROJECT_NAME} main.cpp)
apply_engine_lean_flags(${PROJECT_NAME})
apply_engine_lean_link(${PROJECT_NAME})

# Link the executable with the library
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

add_subdirectory(test)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/docs)
add_subdirectory(bench)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
