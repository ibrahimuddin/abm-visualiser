#include "webgpu-utils.h"
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <cstdlib> // For rand()
#include <chrono>
#include "imgui.h"
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include "WebGPUContext.h"
#include "Renderer.h"


#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif 

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif 

#include <iostream>
#include <cassert>
#include <vector>

//  export SDL_VIDEODRIVER=x11
// export WAYLAND_DISPLAY=

const char* shaderSource = R"(

// structure of one of the vertex's. must match vertex buffer layout 
struct VertexInput {
	@location(0) position: vec2f, // two f32 values (x,y). give me whatever the cpu binds to slot 0 and call it position
	@location(1) biologicalData: vec3f, //(r,g,b) each vertex has its own colour
};
struct VertexOutput {
	@builtin(position) position: vec4f, // (x,y,z,w) where the vertex lies on the screen
	// The location here does not refer to a vertex attribute, it just means
	// that this field must be handled by the rasterizer.
	// (It can also refer to another field of another struct that would be used
	// as input to the fragment shader.)
	@location(0) color: vec3f, // vertex shader outputs a colour -> rasterizer interpolates it -> fragment shader receives it
};
@vertex // vertex shader. x,y,r,g,b (from vertex buffer) --> vec4f for gpu
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
    let aspectRatio = 1280.0 / 720.0;
    let correctedX = in.position.x / aspectRatio;
	out.position = vec4f(correctedX, in.position.y, 0.0, 1.0); // in.position is x,y, make z and w 0 and 1.
	out.color = in.biologicalData;
	return out;
}

@fragment // fragment shader. colour from vertex shader --> vec4f representing final rgba colour for every pixel. actually displays the colour.
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0);
}
)";

class Application {
public:
    bool Initialise();
    void Terminate();
    void MainLoop();
    bool IsRunning();
    Application() : window(nullptr), renderer(gpu){}

private:
    GLFWwindow *window;
    WebGPUContext gpu;
    Renderer renderer;
    // wgpu::RenderPipeline pipeline;
    // wgpu::Buffer vertexBuffer;
	uint32_t vertexCount;

    std::vector<Agent> agents;
    int currentScale = 10;
    int stepCounter = 0;
    double totalTime = 0;

    float displayTimer = 0.0f;
    const float TIME_PER_SCALE = 3.0f;
    std::chrono::high_resolution_clock::time_point lastFrameTime;
};

bool Application::Initialise() {
    if (!glfwInit()) return false;
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); 
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(1280, 720, "ABM Visualisation via WebGPU", nullptr, nullptr);
    
    if (!window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        return false;
    }
    if (!gpu.Initialise(window)) {
        return false; 
    }
    
    renderer.InitialisePipeline(shaderSource);
    renderer.InitialiseBuffers(currentScale);
    lastFrameTime = std::chrono::high_resolution_clock::now();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_Init(gpu.getDevice(), 3, gpu.getSurfaceFormat(),  wgpu::TextureFormat::Undefined);

    return true;
}

void Application::Terminate() {
#if defined(WEBGPU_BACKEND_WGPU)
    gpu.getDevice().poll(true); // 'true' forces the CPU to wait until the GPU is idle
#elif defined(WEBGPU_BACKEND_DAWN)
    gpu.getDevice().tick();
#endif
    renderer.Terminate();
    // vertexBuffer.release();
    // pipeline.release();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::MainLoop() {
    if (glfwWindowShouldClose(window)) return;

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Controls");
    static float zoom = 1.0f;
    ImGui::SliderFloat("Zoom", &zoom, 0.1f, 5.0f); 
    ImGui::End();

    glfwPollEvents();

    auto currentFrameTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;

    auto startBench = std::chrono::high_resolution_clock::now();
    renderer.UpdateAgents(); 
    auto endBench = std::chrono::high_resolution_clock::now();

    if (stepCounter < 100) {
        std::chrono::duration<double, std::milli> elapsed = endBench - startBench;
        totalTime += elapsed.count();
        stepCounter++;
    }

    displayTimer += deltaTime.count(); // Accumulate real-world seconds

    // Transition condition: 100 frames measured AND 3 seconds have passed
    if (stepCounter >= 100 && displayTimer >= TIME_PER_SCALE) {
        double avg = totalTime / 100.0;
        std::cout << "COMPLETED SCALE: " << currentScale << " | AVG MATH TIME: " << avg << "ms" << std::endl;

        // Reset for next level
        stepCounter = 0;
        totalTime = 0.0;
        displayTimer = 0.0f;
        currentScale *= 10;

        if (currentScale <= 100000) {
            renderer.InitialiseBuffers(currentScale); // This resets the 'agents' vector and creates a bigger GPU buffer
        } else {
            std::cout << "Objective 1: All scales tested successfully." << std::endl;
            glfwSetWindowShouldClose(window, true);
            return;
        }
    }
    renderer.Draw();
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}


int main() {
    Application application;
    if (!application.Initialise()){
        return 1;
    }

#ifdef __EMSCRIPTEN__
    auto callback = [](void *arg) {
        Application* pApp = reinterpret_cast<Application*>(arg);
        pApp->MainLoop();
    };
    emscripten_set_main_loop_arg(callback, &application, 0, true);
#else 
    while (application.IsRunning()) {
        application.MainLoop();
    }
#endif 

    application.Terminate();
    return 0;
}