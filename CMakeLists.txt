cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME "coroutines")

project("${PROJECT_NAME}" VERSION 1.0 LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD 20)
set(SOURCE_PATH "${PROJECT_SOURCE_DIR}/source")

file(GLOB_RECURSE SOURCE_FILES LIST_DIRECTORIES false "${SOURCE_PATH}/*.c*" "${SOURCE_PATH}/*.h*" )

add_executable("${PROJECT_NAME}" ${SOURCE_FILES})
set_target_properties("${PROJECT_NAME}" PROPERTIES LINKER_LANGUAGE CXX)


target_compile_definitions(${PROJECT_NAME} PRIVATE
        "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING;"
        "_CRT_SECURE_NO_WARNINGS;"
        "NOMINMAX;"
)

find_package(unofficial-concurrentqueue CONFIG REQUIRED)
target_link_libraries("${PROJECT_NAME}" PRIVATE unofficial::concurrentqueue::concurrentqueue)