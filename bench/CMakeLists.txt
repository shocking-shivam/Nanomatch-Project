set(LOB_BENCH_SOURCES
    ${CMAKE_SOURCE_DIR}/Limit_Order_Book/Book.cpp
    ${CMAKE_SOURCE_DIR}/Limit_Order_Book/Limit.cpp
    ${CMAKE_SOURCE_DIR}/Limit_Order_Book/Order.cpp
)

set(LOB_BENCH_COMPILE_FLAGS
    -O3
    -march=native
    -fno-exceptions
    -fno-rtti
)

add_library(lob_stl STATIC ${LOB_BENCH_SOURCES})
target_compile_definitions(lob_stl PRIVATE USE_STL_ALLOC)
target_compile_options(lob_stl PRIVATE ${LOB_BENCH_COMPILE_FLAGS})
target_include_directories(lob_stl PUBLIC ${CMAKE_SOURCE_DIR})

add_library(lob_pool STATIC ${LOB_BENCH_SOURCES})
target_compile_options(lob_pool PRIVATE ${LOB_BENCH_COMPILE_FLAGS})
target_include_directories(lob_pool PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(bench_tool
    bench.cpp
    baseline_adapter.cpp
)
target_include_directories(bench_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(bench_tool PRIVATE lob_stl lob_pool)

# baseline_adapter.cpp defines USE_STL_ALLOC locally; lob_stl provides baseline::Book symbols.
set_source_files_properties(baseline_adapter.cpp PROPERTIES
    COMPILE_DEFINITIONS USE_STL_ALLOC
)
