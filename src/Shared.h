#ifndef SHARED_H
#define SHARED_H

#ifdef __METAL_VERSION__
#define float3 float3
#define float4 float4
#else
#include <glm/glm.hpp>
using float3 = glm::vec3;
using float4 = glm::vec3;
#endif

struct GPUCamera {
    glm::vec4 position;
    glm::vec4 front;
    glm::vec4 up;
    glm::vec4 right;
    float fov;
    float aspectRatio;
};

// struct GPUSphere {
//     float3 center;
//     float radius;
//     int matID;
//     float3 color;
// };
// struct GPULight {
//     float3 position;
//     float3 color;
//     float intensity;
// };

#endif