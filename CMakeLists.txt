cmake_minimum_required(VERSION 3.10)
project(exe2dll)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Add PEForge library
add_subdirectory(peforge)

# Add exe2dll executable
add_executable(exe2dll
    exe2dll/exe2dll.cpp
)

# Link against PEForge
target_link_libraries(exe2dll PRIVATE peforge)

# Include directories
target_include_directories(exe2dll PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/peforge/include
)

# Set output names based on platform
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(exe2dll PROPERTIES OUTPUT_NAME "exe2dll_x64")
else()
    set_target_properties(exe2dll PROPERTIES OUTPUT_NAME "exe2dll_x86")
endif()

# Add test executable if tests are present
if(EXISTS "${CMAKE_SOURCE_DIR}/test/test.cpp")
    add_executable(exe2dll_test
        test/test.cpp
    )
    target_link_libraries(exe2dll_test PRIVATE peforge)
    target_include_directories(exe2dll_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/peforge/include
    )
endif()

# Windows-specific settings
if(WIN32)
    # Add Windows-specific compile definitions
    target_compile_definitions(exe2dll PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    
    # Set Visual Studio runtime library
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
