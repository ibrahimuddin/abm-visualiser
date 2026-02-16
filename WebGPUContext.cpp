
#include "WebGPUContext.h"
#include "webgpu-utils.h"
#include <glfw3webgpu.h>
#include <iostream>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif


bool WebGPUContext::Initialise(GLFWwindow* window)
{
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
    wgpu::RequiredLimits requiredLimits = getRequiredLimits(adapter);
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
    return true;
}

void WebGPUContext::Terminate()
{
    surface.unconfigure();
    queue.release();
    surface.release();    
    device.release();
}

wgpu::RequiredLimits WebGPUContext::getRequiredLimits(wgpu::Adapter adapter) const {
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

wgpu::TextureView WebGPUContext::GetNextSurfaceTextureView() {
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