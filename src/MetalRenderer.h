#ifndef METAL_RENDERER_H
#define METAL_RENDERER_H

class Camera; // foward declaration of Camera

#ifdef __OBJC__
@class MTLDevice;
@class MTLCommandQueue;
@class MTLComputePipelineState;
@class MTLTexture;
@class MTLBuffer;
#else
typedef void* MTLDevice;
typedef void* MTLCommandQueue;
typedef void* MTLComputePipelineState;
typedef void* MTLTexture;
typedef void* MTLBuffer;
#endif


class MetalRenderer {
public:
    void init(int w, int h);
    void render(const Camera& camera);
    unsigned int getOpenGLTextureID(); // for opengl flow
    void cleanup();

private:
    int width, height;
    unsigned int glTextureID;

    // Metal core objects
    void *device;
    void *commandQueue;
    void *computePipeline;
    void *metalTexture;
    // Data Buffers
    void *cameraBuffer;
    // void *lightBuffer;
    // void *sphereBuffer;
};

#endif