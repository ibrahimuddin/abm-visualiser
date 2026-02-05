#include "webgpu-utils.h"
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

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
@vertex
fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
    return vec4f(in_vertex_position, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(0.0, 0.4, 1.0, 1.0);
}
)";

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

private:
    GLFWwindow *window;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Surface surface;
    wgpu::RenderPipeline pipeline;
    wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
    wgpu::Buffer vertexBuffer;
	uint32_t vertexCount;
};

bool Application::Initialise() {
    if (!glfwInit()) return false;
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); 
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
    
    if (!window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        return false;
    }

    wgpu::InstanceDescriptor desc = wgpu::Default;
    wgpu::Instance instance = wgpu::createInstance(desc);

    std::cout << "Requesting adapter..." << std::endl;
    surface = glfwGetWGPUSurface(instance, window);
    
    wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
    adapterOpts.compatibleSurface = surface;
	wgpu::Adapter adapter = (WGPUAdapter)requestAdapterSync((WGPUInstance)instance, (const WGPURequestAdapterOptions*)&adapterOpts);	    
    std::cout << "Got adapter: " << adapter << std::endl;
    
    instance.release();

    std::cout << "Requesting device..." << std::endl;
    wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
    deviceDesc.label = "Ibrahim's Device";
    deviceDesc.defaultQueue.label = "default queue";
    wgpu::RequiredLimits requiredLimits = GetRequiredLimits(adapter);
    deviceDesc.requiredLimits = &requiredLimits;
	device = requestDeviceSync(adapter, (const WGPUDeviceDescriptor*)&deviceDesc);
    std::cout << "Got device: " << device << std::endl;

    // The C++ method style:
    queue = device.getQueue();

    // Configure the surface
    wgpu::SurfaceConfiguration config = wgpu::Default;
    config.width = 640;
    config.height = 480;
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
    glfwPollEvents();

    wgpu::TextureView targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
    encoderDesc.label = "Ibrahim's command encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

    wgpu::RenderPassDescriptor renderPassDesc = wgpu::Default;

    wgpu::RenderPassColorAttachment renderPassColorAttachment = wgpu::Default;
    renderPassColorAttachment.view = targetView;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{ 0.9, 0.1, 0.2, 1.0 };
    
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    
    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    
    // Select which render pipeline to use
	renderPass.setPipeline(pipeline);

    renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexBuffer.getSize());

	// Draw 1 instance of a 3-vertices shape
	renderPass.draw(vertexCount, 1, 0, 0);
    
    renderPass.end();
    renderPass.release();

    wgpu::CommandBufferDescriptor cmdBufferDescriptor = wgpu::Default;
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();

    std::cout << "Submitting command..." << std::endl;
    queue.submit(1, &command);
    command.release();
    
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
	// Set the chained struct's header
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
	// Connect the chain
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	shaderCodeDesc.code = shaderSource;
	wgpu::ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    wgpu::RenderPipelineDescriptor pipelineDesc;

    // Configure the vertex pipeline
	// We use one vertex buffer
	wgpu::VertexBufferLayout vertexBufferLayout;
	wgpu::VertexAttribute positionAttrib;
	// == For each attribute, describe its layout, i.e., how to interpret the raw data ==
	// Corresponds to @location(...)
	positionAttrib.shaderLocation = 0;
	positionAttrib.format = wgpu::VertexFormat::Float32x2;
	// Index of the first element
	positionAttrib.offset = 0;
	vertexBufferLayout.attributeCount = 1;
	vertexBufferLayout.attributes = &positionAttrib;
	// == Common to attributes from the same buffer ==
	vertexBufferLayout.arrayStride = 2 * sizeof(float);
	vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
	
	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;

    pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

    // make into triangle
	pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	// The face orientation is defined by assuming that when looking
	// from the front of the face, its corner vertices are enumerated
	// in the counter-clockwise (CCW) order.
	pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

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
	wgpu::RequiredLimits requiredLimits = wgpu::Default;
	requiredLimits.limits.maxVertexAttributes = 1;
	requiredLimits.limits.maxVertexBuffers = 1;
	// Maximum size of a buffer is 6 vertices of 2 float each
	requiredLimits.limits.maxBufferSize = 6 * 2 * sizeof(float);
	requiredLimits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);

	// These two limits are different because they are "minimum" limits,
	// they are the only ones we are may forward from the adapter's supported
	// limits.
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

	return requiredLimits;
}

void Application::InitialiseBuffers(){
    std::vector<float> vertexData = {
    -0.5, -0.5,
    +0.5, -0.5,
    +0.0, +0.5,

    -0.55f, -0.5,
    -0.05f, +0.5,
    -0.55f, +0.5
};
    vertexCount = static_cast<uint32_t>(vertexData.size() / 2);

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = vertexData.size() * sizeof(float);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; 
    bufferDesc.mappedAtCreation = false;
    vertexBuffer = device.createBuffer(bufferDesc);

    queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);
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