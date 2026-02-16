cmake_minimum_required(VERSION 3.0...3.25)
project(
    LearnWebGPU # name of the project, which will also be the name of the visual studio solution if you use it
    VERSION 0.1.0 # any version number
    LANGUAGES CXX C # programming languages used by the project
)

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)
add_subdirectory(imgui)

add_executable(ABM_Visualizer 
main.cpp
webgpu-utils.h
webgpu-utils.cpp
WebGPUContext.h
WebGPUContext.cpp
Renderer.h
Renderer.cpp
)

target_link_libraries(ABM_Visualizer PRIVATE webgpu glfw glfw3webgpu imgui)

target_copy_webgpu_binaries(ABM_Visualizer)

set_target_properties(ABM_Visualizer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(ABM_Visualizer PRIVATE /W4)
else()
    target_compile_options(ABM_Visualizer PRIVATE -Wall -Wextra -pedantic)
endif()

if (XCODE)
    set_target_properties(ABM_Visualizer PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
    )
endif()

if (EMSCRIPTEN)
	set_target_properties(ABM_Visualizer PROPERTIES SUFFIX ".html")
	# Enable the use of emscripten_sleep()
	target_link_options(ABM_Visualizer PRIVATE -sASYNCIFY)
endif()

if (NOT EMSCRIPTEN)
    add_subdirectory(glfw)
else()
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()



