cmake_minimum_required(VERSION 3.29.0)

set(This LimitOrderBookTests)

set(Sources
    LimitOrderBookTests.cpp
    ExampleOrdersTests.cpp
    spsc_tests.cpp
)

add_executable(${This} ${Sources})

# Google Test needs exceptions/RTTI; do not inherit engine lean flags (-fno-exceptions, etc.).
target_compile_options(${This} PRIVATE -O2)

target_link_libraries(${This} PUBLIC
    gtest_main
    LimitOrderBook_lib
    pthread
)

add_test(
    NAME ${This}
    COMMAND ${This}
)
