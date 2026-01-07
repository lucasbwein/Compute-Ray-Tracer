// Objective-C++ extention
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Foundation/Foundation.h>
#import <iostream>
#import <glad/glad.h>

#import "MetalRenderer.h"
#import "Camera.h"
#import "Shared.h"

int MAX_SPHERE = 4;

GPUCamera toGPU(const Camera& cam, int width, int height) {
    return GPUCamera{
        {cam.Position.x, cam.Position.y, cam.Position.z, 0.0f},
        {cam.Front.x, cam.Front.y, cam.Front.z, 0.0f},
        {cam.Up.x, cam.Up.y, cam.Up.z, 0.0f},
        {cam.Right.x, cam.Right.y, cam.Right.z, 0.0f},
        glm::radians(cam.Fov),
        (float)width/(float)height
    };
}

unsigned int MetalRenderer::getOpenGLTextureID() {
    return glTextureID;
}

void MetalRenderer::init(int w, int h) {
    // Get Metal Device
    width = w;
    height = h;

    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    device = (__bridge void*)dev; // Stores in member var

    if (!device) {
        std::cout << "Error on Device creation" << std::endl; 
    }
    
    // Create Command Queue
    id<MTLDevice> deviceObj = (__bridge id<MTLDevice>)device; // cast device to use

    id<MTLCommandQueue> queue = [deviceObj newCommandQueue]; 
    commandQueue = (__bridge void*)queue; // Store in member var

    // load and compile .metal file
    // a) Read file as stirng
    NSError *error = nil;
    NSString *shaderPath = @"/Users/lucasweinstein/Projects/Personal/OpenGl_Ray_Tracing/shaders/rayTracer.metal";
    NSString *shaderSource = [NSString stringWithContentsOfFile:shaderPath
                            encoding:NSUTF8StringEncoding
                            error:&error];
    if(!shaderSource) {
        NSLog(@"Failed to load shader file: %@", error);
        NSLog(@"Tried path: %@", shaderPath);
        NSLog(@"Current directory: %@", [[NSFileManager defaultManager] currentDirectoryPath]);
        return;
    }
    NSLog(@"\nShader Source Success");

    // MTLCompileOptions *options = [[MTLCompileOptions alloc]init];
    // options.additionalCompilerArguments = @[@"-I", @"/Users/lucasweinstein/Projects/Personal/OpenGl_Ray_Tracing/src"];
    // b) compile shader into library
    id<MTLLibrary> library = [deviceObj newLibraryWithSource:shaderSource
                            options:nil
                            error:&error];
    if(!library) {
        NSLog(@"Library Failed: %@", error);
    }
    NSLog(@"\nLibrary Success");

    // Create compute pipeline
    // a) Get kernel function from library
    id<MTLFunction> kernelFunction = [library newFunctionWithName:@"rayTrace"];
    if(!kernelFunction) {
        NSLog(@"Kernel Failed: %@", error);
    }
    NSLog(@"\nKernel Success");

    // b) Create pipeline state
    id<MTLComputePipelineState> computePipe = [deviceObj 
                                newComputePipelineStateWithFunction:kernelFunction error:&error];
    computePipeline = (__bridge void*)computePipe; // Stores in member var

    // Create Metal Texture
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor 
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
        width:width
        height:height
        mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;

    id<MTLTexture> metalTex = [deviceObj newTextureWithDescriptor:descriptor];
    metalTexture = (__bridge void*)metalTex; // Stores in member var

    // Create OpenGL texture
    glGenTextures(1, &glTextureID);
    glBindTexture(GL_TEXTURE_2D, glTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);


    id<MTLBuffer> cameraBuf = [deviceObj newBufferWithLength:sizeof(GPUCamera)
                                    options:MTLResourceStorageModeShared];
    cameraBuffer = (__bridge void*)cameraBuf;
    // id<MTLBuffer> lightBuf = [device newBufferWithLength:sizeof(GPULight) * MAX_LIGHTS
    //                                 options:MTLResourceStorageModeShared];
    // lightBuffer = (__bridge void*)lightBuf;
        // id<MTLBuffer> sphereBuf = [device newBufferWithLength:sizeof(GPUSphere) * MAX_SPHERE
    //                                 options:MTLResourceStorageModeShared];
    // sphereBuffer = (__bridge void*)sphereBuf;
}

void MetalRenderer::render(const Camera& camera) {
    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
    id<MTLComputePipelineState> pipeline = (__bridge id<MTLComputePipelineState>)computePipeline;
    id<MTLTexture> texture = (__bridge id<MTLTexture>)metalTexture;
    // id<MTLBuffer> sphereBuf = (__bridge id<MTLBuffer>)sphereBuffer;
    // id<MTLBuffer> lightBuf = (__bridge id<MTLBuffer>)cameraBuffer;
    id<MTLBuffer> cameraBuf = (__bridge id<MTLBuffer>)cameraBuffer;

    GPUCamera gpuCam = toGPU(camera, width, height);
    memcpy([cameraBuf contents], &gpuCam, sizeof(GPUCamera));

    // Step 1: Create command buffer
    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    
    // Step 2: Create compute encoder
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    
    // Step 3: Set pipeline and resources
    [encoder setComputePipelineState:pipeline]; // Bind pipeline
    [encoder setTexture:texture atIndex:0]; // bind texture

    [encoder setBuffer:cameraBuf offset:0 atIndex:0]; // bind camera
    // [encoder setBuffer:lightBuffer offset:0 atIndex:2]; // bind light
    // [encoder setBuffer:sphereBuffer offset:0 atIndex:0]; // bind sphere buffer

    // Step 4: Dispatch threads
    MTLSize gridSize = MTLSizeMake(width, height, 1);
    MTLSize threadgroupSize = MTLSizeMake(8, 8, 1);
    [encoder dispatchThreads:gridSize
        threadsPerThreadgroup:threadgroupSize];
    
    // Step 5: End encoding and execute
    [encoder endEncoding]; // ends encoding
    [commandBuffer commit]; // Executes buffer
    
    // Step 6: Wait for completion
    [commandBuffer waitUntilCompleted];
    
    // Step 7: Copy to OpenGL texture
    std::vector<uint8_t> pixelData(width * height * 4);  // RGBA = 4 bytes per pixel
    [texture getBytes:pixelData.data()
            bytesPerRow:width * 4
            fromRegion:MTLRegionMake2D(0, 0, width, height)
            mipmapLevel:0];
    // Bind data to openGL Texture
    glBindTexture(GL_TEXTURE_2D, glTextureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());
}