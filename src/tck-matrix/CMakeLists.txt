# TChecker matrix visualization tool

tck_add_executable(tck-matrix
    tck-matrix.cc
)

target_link_libraries(tck-matrix libtchecker)
target_include_directories(tck-matrix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})