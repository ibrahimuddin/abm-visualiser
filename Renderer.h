#pragma once
#include "WebGPUContext.h"
#include <vector>

struct Agent {
    // spatial prop's
    float x,y,dx,dy;

    // biological prop's

    float proteinLevel;
    float greediness;
    float speed;
};

class Renderer {
public:
    Renderer(WebGPUContext& ctx);

    void InitialisePipeline(const char* shaderSource);
    void InitialiseBuffers(int scale);
    void UpdateAgents();
    void Draw();
    void Terminate();

    void SetWindowSize(int width, int height);

private:
    WebGPUContext& context;

    wgpu::RenderPipeline pipeline;
    wgpu::Buffer vertexBuffer;
    uint32_t vertexCount;

    int windowWidth = 1280;
    int windowHeight = 720;

    const float agentSize = 0.01f;

    //temp
    std::vector<Agent> agents;
    int currentScale = 1000; 
};
