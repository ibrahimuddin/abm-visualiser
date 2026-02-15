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

struct Agent {
    // spatial prop's
    float x,y,dx,dy;

    // biological prop's

    float proteinLevel;
    float greediness;
    float speed;
};

class Application {
public:
    bool Initialise();
    void Terminate();
    void MainLoop();
    bool IsRunning();

private:
    wgpu::TextureView GetNextSurfaceTextureView();
    void InitialisePipeline();
    wgpu::RequiredLimits GetRequiredLimits(wgpu::Adapter adapter) const;
    void InitialiseBuffers();
    void UpdateAgents();

private:
    GLFWwindow *window;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Surface surface;
    wgpu::RenderPipeline pipeline;
    wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
    wgpu::Buffer vertexBuffer;
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

    wgpu::InstanceDescriptor desc = wgpu::Default;
    wgpu::Instance instance = wgpu::createInstance(desc); // entry point to the WebGPU library

    std::cout << "Requesting adapter..." << std::endl;
    surface = glfwGetWGPUSurface(instance, window); // where pixels will be drawn ('canvas')
    
    wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
    adapterOpts.compatibleSurface = surface;
	wgpu::Adapter adapter = (WGPUAdapter)requestAdapterSync((WGPUInstance)instance, (const WGPURequestAdapterOptions*)&adapterOpts); //actual gpu. tells us what the hardware is capable of
    std::cout << "Got adapter: " << adapter << std::endl;
    
    instance.release();

    std::cout << "Requesting device..." << std::endl;
    wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
    deviceDesc.label = "Ibrahim's Device";
    deviceDesc.defaultQueue.label = "default queue";
    wgpu::RequiredLimits requiredLimits = GetRequiredLimits(adapter);
    // requiredLimits.limits.maxBindGroups = 2;
    deviceDesc.requiredLimits = &requiredLimits;
	device = requestDeviceSync(adapter, (const WGPUDeviceDescriptor*)&deviceDesc); // active session. its the engine. Used to create the buffers, pipelines, shaders
    std::cout << "Got device: " << device << std::endl;

    queue = device.getQueue();

    // Configure the surface
    wgpu::SurfaceConfiguration config = wgpu::Default;
    config.width = 1280;
    config.height = 720;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    surfaceFormat = surface.getPreferredFormat(adapter);
    config.format = surfaceFormat;
    config.device = device;
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;
    
    surface.configure(config);

    adapter.release();
    InitialisePipeline();
    InitialiseBuffers();
    lastFrameTime = std::chrono::high_resolution_clock::now();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_Init(device, 3, surfaceFormat,  wgpu::TextureFormat::Undefined);

    return true;
}

void Application::Terminate() {
    vertexBuffer.release();
    pipeline.release();
    surface.unconfigure();
    queue.release();
    surface.release();    
    device.release();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::MainLoop() {

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
    UpdateAgents(); 
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
            InitialiseBuffers(); // This resets the 'agents' vector and creates a bigger GPU buffer
        } else {
            std::cout << "Objective 1: All scales tested successfully." << std::endl;
            glfwSetWindowShouldClose(window, true);
        }
    }

    wgpu::TextureView targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    wgpu::CommandEncoder encoder = device.createCommandEncoder(wgpu::Default);
    wgpu::RenderPassColorAttachment colorAttachment = wgpu::Default;
    colorAttachment.view = targetView;
    colorAttachment.loadOp = wgpu::LoadOp::Clear; //wipe the screen with a very dark grey colour
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    colorAttachment.clearValue = { 0.05, 0.05, 0.05, 1.0 };

    //drawing session, math already done but hasn't been drawn yet
    wgpu::RenderPassDescriptor renderPassDesc = wgpu::Default;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;

    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.setPipeline(pipeline);
    // Use vertexCount * 5 * 4 bytes to ensure we bind the correct size
    renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 5 * sizeof(float));
    renderPass.draw(vertexCount, 1, 0, 0);

    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);

    renderPass.end();

    wgpu::CommandBuffer command = encoder.finish(wgpu::Default);
    queue.submit(1, &command);
    
    targetView.release();
#ifndef __EMSCRIPTEN__
    surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    device.poll(false);
#endif
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}

wgpu::TextureView Application::GetNextSurfaceTextureView() {
    wgpu::SurfaceTexture surfaceTexture;
    surface.getCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
        return nullptr;
    }
	wgpu::Texture texture = (wgpu::Texture)surfaceTexture.texture;    wgpu::TextureViewDescriptor viewDescriptor = wgpu::Default;
    viewDescriptor.label = "Surface texture view";
    viewDescriptor.format = texture.getFormat();
    viewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = wgpu::TextureAspect::All;
    
    wgpu::TextureView targetView = texture.createView(viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
    texture.release();
#endif 

    return targetView;
}

void Application::InitialisePipeline() {

    wgpu::ShaderModuleDescriptor shaderDesc;
#ifdef WEBGPU_BACKEND_WGPU
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
#endif

	// We use the extension mechanism to specify the WGSL part of the shader module descriptor
	wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	shaderCodeDesc.code = shaderSource;
	wgpu::ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    wgpu::RenderPipelineDescriptor pipelineDesc;

    // Configure the vertex pipeline
	// We use one vertex buffer
	wgpu::VertexBufferLayout vertexBufferLayout;
    std::vector<wgpu::VertexAttribute> vertexAttribs(2);

    vertexAttribs[0].shaderLocation = 0; // @location(0)
    vertexAttribs[0].format = wgpu::VertexFormat::Float32x2;
    vertexAttribs[0].offset = 0;

    vertexAttribs[1].shaderLocation = 1; // @location(1)
    vertexAttribs[1].format = wgpu::VertexFormat::Float32x3;
    vertexAttribs[1].offset = 2 * sizeof(float); // non null offset!. as in [x,y,r,g,b,x,y,r,g,b....] we want to skip the x and y

    vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
    vertexBufferLayout.attributes = vertexAttribs.data();
	vertexBufferLayout.arrayStride = 5 * sizeof(float); //(x,y,r,g,b) once you've finished with one vertex, jump foreward exactly 5 numbers to find the next and because of the offset being 2*..., it skips the x,y each time
	vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
	
	pipelineDesc.vertex.bufferCount = 1; // for every vertex, i will give 1 physical buffer
	pipelineDesc.vertex.buffers = &vertexBufferLayout; 

    pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

    // make into triangle
	pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;// take every 3 vertiecs and make into seperate triangle
	pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	pipelineDesc.primitive.cullMode = wgpu::CullMode::None; //when triangle rotates, it never disappears from screen

    // colouring of every pixel
    wgpu::FragmentState fragmentState;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

    pipelineDesc.fragment = &fragmentState;

    wgpu::BlendState blendState;
	blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = wgpu::BlendOperation::Add;
	blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blendState.alpha.dstFactor = wgpu::BlendFactor::One;
	blendState.alpha.operation = wgpu::BlendOperation::Add;
	
	wgpu::ColorTargetState colorTarget;
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = wgpu::ColorWriteMask::All; 

    fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;


    // not going to use (used to discard fragments)
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
	// Default value for the mask, meaning "all bits on"
	pipelineDesc.multisample.mask = ~0u;
	// Default value as well (irrelevant for count = 1 anyways)
	pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipelineDesc.layout = nullptr;
    pipeline = device.createRenderPipeline(pipelineDesc);
    shaderModule.release();

}

wgpu::RequiredLimits Application::GetRequiredLimits(wgpu::Adapter adapter) const {
	wgpu::SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);
	wgpu::RequiredLimits requiredLimits = {};
    requiredLimits.limits = supportedLimits.limits;

    // requiredLimits.limits.maxTextureDimension1D = supportedLimits.limits.maxTextureDimension1D;
    // requiredLimits.limits.maxTextureDimension2D = supportedLimits.limits.maxTextureDimension2D;
    // requiredLimits.limits.maxTextureArrayLayers = supportedLimits.limits.maxTextureArrayLayers;

	requiredLimits.limits.maxVertexAttributes = 8;
	requiredLimits.limits.maxVertexBuffers = 1;
	// Maximum size of a buffer is 6 vertices of 2 float each
	requiredLimits.limits.maxBufferSize = 100000 * 3 * 5 * sizeof(float);
	requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float); //x,y,r,g,b
    // requiredLimits.limits.maxInterStageShaderComponents = 3;

	// These two limits are different because they are "minimum" limits,
	// they are the only ones we are may forward from the adapter's supported
	// limits.
	// requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	// requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

	return requiredLimits;
}

void Application::InitialiseBuffers(){
    if (vertexBuffer) vertexBuffer.release(); //called every time our scale changes 10--100--1000 etc so must destroy old gpu buffer
    agents.clear(); // both prevent memory leaks
    //float scale = 0.02f;

    // generate starting state for every agent
    for (int i=0; i<currentScale; ++i){
        float x = ((float)rand() / (float)RAND_MAX) * 1.8f - 0.9f;
        float y = ((float)rand() / (float)RAND_MAX) * 1.8f - 0.9f;

        float protein = (float)rand() / (float)RAND_MAX;
        float greed = (float)rand() / (float)RAND_MAX;
        float speed =  (float)rand() / (float)RAND_MAX + 0.05f;

        float dx = ((float)rand() / (float)RAND_MAX -0.5f) * 0.01f;
        float dy = ((float)rand() / (float)RAND_MAX -0.5f) * 0.01f;
        agents.push_back({x,y,dx,dy, protein, greed, speed});
    }

    vertexCount = currentScale * 3; //as 3 vertices

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = vertexCount * 5 * sizeof(float); //as every vertex needs 5 floats x,y,r,g,b x4 bytes for size of float
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; 
    // bufferDesc.mappedAtCreation = false;
    vertexBuffer = device.createBuffer(bufferDesc);

    // queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);
}

void Application::UpdateAgents(){
    // go through agents db and pick our exactly what the gpu needs (position and colour)
    std::vector<float> vertexData;
    vertexData.reserve(vertexCount * 5);
    float s = 0.01f; //radius of agent triangles
    //float triangleSize = 0.01f;

    for (auto&a : agents) {
        // euler integration  
        a.x +=a.dx * a.speed;
        a.y +=a.dy * a.speed;

        if (a.greediness > 0.8f){
            a.x += ((float)rand() / (float)RAND_MAX - 0.5f) * 0.002f;
            a.y += ((float)rand() / (float)RAND_MAX - 0.5f) * 0.002f;
        }
    
        // 1280 / 720  roughtly 1.77
        if (a.x > 1.77f) a.x = -1.77f; else if (a.x < -1.77f) a.x = 1.77f; //if agent walsk off the right, popped back to the left
        if (a.y > 1.0f) a.y = -1.0f; else if (a.y < -1.0f) a.y = 1.0f;
        vertexData.insert(vertexData.end(), std::initializer_list<float>{ 
        a.x - s, a.y - s, 0.2f, a.proteinLevel, a.greediness, // Vertex 1
        a.x + s, a.y - s, 0.2f, a.proteinLevel, a.greediness, // Vertex 2
        a.x,     a.y + s, 0.2f, a.proteinLevel, a.greediness  // Vertex 3
    });
    }
    queue.writeBuffer(vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(float));
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