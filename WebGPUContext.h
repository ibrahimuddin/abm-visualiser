#pragma once
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp> 

class WebGPUContext {
public:
    bool Initialise(GLFWwindow* window);
    void Terminate();

    wgpu::Device getDevice() const { return device; }
    wgpu::Queue getQueue() const { return queue; }
    wgpu::Surface getSurface() const { return surface; }
    wgpu::TextureFormat getSurfaceFormat() const { return surfaceFormat; }
    wgpu::TextureView GetNextSurfaceTextureView();

private:
    wgpu::RequiredLimits getRequiredLimits(wgpu::Adapter adapter) const;

private:
    wgpu::Device device =nullptr;
    wgpu::Queue queue =nullptr;
    wgpu::Surface surface= nullptr;
    wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
};