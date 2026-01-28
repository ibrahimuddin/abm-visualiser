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
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

class Application {
public:
    bool Initialise();
    void Terminate();
    void MainLoop();
    bool IsRunning();

private:
	WGPUTextureView GetNextSurfaceTextureView();

private:
	GLFWwindow *window;
    wgpu::Device device;
    wgpu::Queue queue;
	wgpu::Surface surface;
};

bool Application::Initialise() {
	if (!glfwInit()) return false;
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); 
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
	
	if (!window) {;
		std::cerr << "Could not open window!" << std::endl;
		glfwTerminate();
		return 1;
	}
	wgpu::InstanceDescriptor desc = {};
	wgpu::Instance instance = wgpuCreateInstance(desc);

	std::cout << "Requesting adapter..." << std::endl;
	wgpu::RequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	surface = glfwGetWGPUSurface(instance, window);
	adapterOpts.compatibleSurface = surface;
	wgpu::Adapter adapter = requestAdapterSync(instance, adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;
	inspectAdapter(adapter);
	wgpuInstanceRelease(instance);

	// We no longer need to use the instance once we have the adapter
	// without these = memory leak
	std::cout << "Requesting device..." << std::endl;
	wgpu::DeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "Ibrahim's Device"; // used in error messages, only used by Dawn
	deviceDesc.requiredFeatureCount = 0; 
	deviceDesc.requiredLimits = nullptr; 
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "default queue";
	deviceDesc.deviceLostCallback = nullptr;
	device = requestDeviceSync(adapter, &deviceDesc);
	inspectDevice(device);
	std::cout << "Got device: " << device << std::endl;

	// conveyor built connecting CPU and GPU
	queue = wgpuDeviceGetQueue(device);

	auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* pUserData */) {
        std::cout << "GPU finished the work! Status: " << status << std::endl;
    };
    
    // We register it here, but it won't actually "fire" until we submit work later
    wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr);

	WGPUSurfaceConfiguration config = {};
	config.nextInChain = nullptr;

	config.width = 640;
	config.height = 480;
	config.usage = WGPUTextureUsage_RenderAttachment; //telling the gpu I am going to use this texture as a canvas to draw pixels into
	WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
	config.format = surfaceFormat; //B-G-R-A: The order is Blue, then Green, then Red, then Alpha.
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = device;
	config.presentMode = WGPUPresentMode_Fifo;
	config.alphaMode = WGPUCompositeAlphaMode_Auto;
	wgpuSurfaceConfigure(surface, &config);

	wgpuAdapterRelease(adapter);
	return true;

}

void Application::Terminate() {
	wgpuSurfaceUnconfigure(surface);
	wgpuQueueRelease(queue);
	wgpuSurfaceRelease(surface);	
	wgpuDeviceRelease(device);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::MainLoop() {
	glfwPollEvents();

	WGPUTextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;
		// a 'recorder' that we use to write down the instructions as we cant talk to queue directly 
	// (can't directly create buffer object so have to create encoder first, like 'draw a circle')
	// we want to record where each cell moves, encoder will be used to record a compute Pass
	// 'take these 100000 agents and run the 'movement shader' on all at once
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "Ibrahim's command encoder";
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

	wgpuCommandEncoderInsertDebugMarker(encoder, "Do one thing");
	wgpuCommandEncoderInsertDebugMarker(encoder, "Do another thing");

	WGPURenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;

	WGPURenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;


	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
	wgpuRenderPassEncoderEnd(renderPass);
	wgpuRenderPassEncoderRelease(renderPass);

	// the finished list of our instructions created by encoder
	// when agent data changes on the cpu, we need to use the conveyor belt to shoot down the 
	// updated info to gpu 
	WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.nextInChain = nullptr;
	cmdBufferDescriptor.label = "Command buffer";
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
	wgpuCommandEncoderRelease(encoder);

	std::cout << "Submitting command..." << std::endl;
	wgpuQueueSubmit(queue, 1, &command);
	wgpuCommandBufferRelease(command);
	std::cout << "Command submitted." << std::endl;
	wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
	wgpuSurfacePresent(surface);
#endif

#if defined(WEBGPU_BACKEND_DAWN)
		wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
		wgpuDevicePoll(device, false, nullptr);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
		emscripten_sleep(100);
#endif
}

bool Application::IsRunning() {
	return !glfwWindowShouldClose(window);
}

WGPUTextureView Application::GetNextSurfaceTextureView() {
	// Get the surface texture
	WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
	if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
		return nullptr;
	}

	// Create a view for this surface texture
	WGPUTextureViewDescriptor viewDescriptor;
	viewDescriptor.nextInChain = nullptr;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
	viewDescriptor.dimension = WGPUTextureViewDimension_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = WGPUTextureAspect_All;
	WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
	// We no longer need the texture, only its view
	// (NB: with wgpu-native, surface textures must not be manually released)
	wgpuTextureRelease(surfaceTexture.texture);
#endif // WEBGPU_BACKEND_WGPU

	return targetView;
}

int main() {
	Application application;
	if (!application.Initialise()){
		return 1;
	}

#ifdef __EMSCRIPTEN__
// Equivalent of the main loop when using Emscripten: 
// writing whole loop directly not possible as it conflicts with the web browsers own loop
	auto callback = [](void *arg) {
		//                   ^^^ 2. We get the address of the app in the callback.
		Application* pApp = reinterpret_cast<Application*>(arg);
		//                  ^^^^^^^^^^^^^^^^ 3. We force this address to be interpreted
		//                                      as a pointer to an Application object.
		pApp->MainLoop(); // 4. We can use the application object
	};
	emscripten_set_main_loop_arg(callback, &application, 0, true);
	//                                     ^^^^ 1. We pass the address of our application object.
#else // __EMSCRIPTEN__
	while (application.IsRunning()) {
		application.MainLoop();
	}
#endif // __EMSCRIPTEN__

    application.Terminate();

    return 0;

	return 0;
}