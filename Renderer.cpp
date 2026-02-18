#include "Renderer.h"
#include <iostream>
#include "imgui.h"
#include <backends/imgui_impl_wgpu.h>
#include <cmath>

Renderer::Renderer(WebGPUContext& context) 
    : context(context), vertexCount(0) {}

void Renderer::InitialisePipeline(const char* shaderSource) {

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
	wgpu::ShaderModule shaderModule = context.getDevice().createShaderModule(shaderDesc);

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
	colorTarget.format = context.getSurfaceFormat();
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
    pipeline = context.getDevice().createRenderPipeline(pipelineDesc);
    shaderModule.release();
}

void Renderer::InitialiseBuffers(int scale){
    if (vertexBuffer) vertexBuffer.release(); //called every time our scale changes 10--100--1000 etc so must destroy old gpu buffer
    agents.clear(); // both prevent memory leaks
    //float scale = 0.02f;

    // generate starting state for every agent
    for (int i=0; i<scale; ++i){
        float x = ((float)rand() / (float)RAND_MAX) * 1.8f - 0.9f;
        float y = ((float)rand() / (float)RAND_MAX) * 1.8f - 0.9f;

        float protein = (float)rand() / (float)RAND_MAX;
        float greed = (float)rand() / (float)RAND_MAX;
        float speed =  (float)rand() / (float)RAND_MAX + 0.05f;

        float dx = ((float)rand() / (float)RAND_MAX -0.5f) * 0.01f;
        float dy = ((float)rand() / (float)RAND_MAX -0.5f) * 0.01f;
        agents.push_back({x,y,dx,dy, protein, greed, speed});
    }

    vertexCount = scale * 3; //as 3 vertices

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = vertexCount * 5 * sizeof(float); //as every vertex needs 5 floats x,y,r,g,b x4 bytes for size of float
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; 
    // bufferDesc.mappedAtCreation = false;
    vertexBuffer = context.getDevice().createBuffer(bufferDesc);

    // queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);
}

void Renderer::UpdateAgents(float zoom, float rotation){
    // go through agents db and pick our exactly what the gpu needs (position and colour)
    std::vector<float> vertexData;
    vertexData.reserve(agents.size() * 3 * 5);
    float baseSize = 0.01f; //radius of agent triangles
    //float triangleSize = 0.01f;
    float s = baseSize * zoom;

    // sin/ cos expect radians so convert degrees to radians
    // radians = degrees * (pi/180)
    
    float pi = 3.142f;
    float radians = rotation * (pi/180.0f);
    float cosAngle = std::cos(radians);
    float sinAngle = std::sin(radians);



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

        // 2d rotation matrix to (x,y)
        // x' = x * cos(angle) - y * sin(angle)
        // y' = y * cos(angle) + x * sin(angle)

        float x = (a.x * cosAngle - a.y * sinAngle) * zoom;
        float y = (a.y * cosAngle + a.x * sinAngle) * zoom;
        
        // local rotation - rotating the corners around the centre
        float x1 = (-s) * cosAngle - (-s) * sinAngle;
        float y1 = (-s) * cosAngle + (-s) * sinAngle;
        float x2 = (s) * cosAngle - (-s) * sinAngle;
        float y2 = (-s) * cosAngle + (s) *sinAngle;
        float x3 = 0 * cosAngle - (s) * sinAngle;
        float y3 = (s) * cosAngle + 0 * sinAngle;

        // float x = a.x * zoom;
        // float y = a.y * zoom;


        vertexData.insert(vertexData.end(), std::initializer_list<float>{ 
        x + x1, y + y1, 0.2f, a.proteinLevel, a.greediness, // Vertex 1
        x + x2 , y + y2, 0.2f, a.proteinLevel, a.greediness, // Vertex 2
        x + x3,     y + y3, 0.2f, a.proteinLevel, a.greediness  // Vertex 3
    });
    }
    context.getQueue().writeBuffer(vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(float));
}

void Renderer::Draw(){
    wgpu::TextureView targetView = context.GetNextSurfaceTextureView();
    if (!targetView) {
        ImGui::EndFrame();
        return;
    }


    wgpu::CommandEncoder encoder = context.getDevice().createCommandEncoder(wgpu::Default);
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
    context.getQueue().submit(1, &command);
    
    targetView.release();
#ifndef __EMSCRIPTEN__
    context.getSurface().present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    context.getDevice().tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    context.getDevice().poll(false);
#endif
}

void Renderer::Terminate() {
    if (pipeline) pipeline.release();
    if (vertexBuffer) vertexBuffer.release();
    // pipeline = nullptr;
    // vertexBuffer = nullptr;
}
