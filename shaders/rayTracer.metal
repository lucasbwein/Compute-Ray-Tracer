// Metal Shading Language
#include <metal_stdlib>
using namespace metal;

// ray trace logic here
struct Ray {
    float3 origin;
    float3 direction;
};

struct Sphere {
    float3 center;
    float radius;
    float3 color;
};
struct Light {
    float3 position;
    float3 color;
};

struct GPUCamera {
    float4 position;
    float4 front;
    float4 up;
    float4 right;
    float fov;
    float aspectRatio;
};

struct Hit {
    bool hit = false;
    float t = 0.0f; // used for how far along until hit
    float3 point; // where the light hit
    float3 normal;
    int matID = -1;
    float3 color;
    float reflectivity;
};
constant int NUM_SPHERES = 3;

/* ===================================
TODO:
Reflection Formula = I - 2 * dot(I, N) * N

Incident ray direction: I (ray hitting surface)
Surface normal: N (perpendicular to surface)

When light bounces:
- Some energy is absorbed by the material (becomes heat)
- Some energy is reflected (continues as light)
- Light gets tinted by the surface color

Ray generateRay(uint2 gid, constant GPUCamera* cam, uint2 gridSize);
float3 traceRay(Ray ray, thread Sphere* spheres, Light light);
bool intersectSphere(Ray ray, Sphere sphere, float tMin, float tMax, thread Hit& hit);

kernel void rayTrace(
    texture2d<float, access::write> output [[texture(0)]],
    constant GPUCamera* camera [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]],
    uint2 gridSize [[threads_per_grid]])
{
    // Define Sphere and Light
    Sphere spheres[NUM_SPHERES] = {
        {{0.0, 0.0, -5.0}, 1.0, {1.0, 0.0, 0.0}},       // Red Sphere
        {{2.2, 0.0, -5.0}, 1.0, {0.0, 1.0, 0.0}},       // Green Sphere
        {{0.0, -101.5, -5.0}, 100.0, {0.5, 0.5, 0.5}}   // Ground (gray)
    };
    Light light = {{5.0, 5.0, 0.0}, {1.0, 1.0, 1.0}};

    // Same typical logic for CPU ray tracing
    // but runs per pixel parallel, more efficient

    // gid.x = x && gid.y = y

    Ray ray = generateRay(gid, camera, gridSize);

    float3 color = traceRay(ray, spheres, light);

    // float3 color = ray.direction * 0.5 + 0.5;
    // float3 color = float3(camera->fov, camera->fov, camera->fov);

    output.write(float4(color, 1.0), gid);
}

Ray generateRay(uint2 gid, constant GPUCamera* cam, uint2 gridSize) {
    Ray genRay;
    // Normalized pixel coordinates to [-1, 1]
    float u = 2.0 * (gid.x + 0.5) / float(gridSize.x) - 1;
    float v = 2.0 * (gid.y + 0.5) / float(gridSize.y) - 1;

    // Calculate scale FOV
    float scale = tan(cam->fov * 0.5f);

    // Calculate offset
    float3 dir_cam = cam->front.xyz 
                    + (u * cam->aspectRatio * scale) * cam->right.xyz
                    + (v * scale) * cam->up.xyz;

    // Generate ray
    genRay.origin = cam->position.xyz;
    genRay.direction = normalize(dir_cam);
    return genRay;
}

float3 traceRay(Ray ray, thread Sphere* spheres, Light light) {
    Hit hit;
    float tMin = 0.001f;
    float tMax = 9999.9f;

    for (int i=0; i < NUM_SPHERES; i++){
    //sphere[i].intersect(ray, tMin, tMax, hit)
        if(intersectSphere(ray, spheres[i], tMin, tMax, hit)) {
            tMax = hit.t;
        }
    }

    if(hit.hit) {
        float3 lightDir = normalize(light.position - hit.point);
        float diffuse = max(dot(hit.normal, lightDir), 0.0f);

        Ray shadowRay;
        shadowRay.direction = lightDir;
        shadowRay.origin = hit.point;

        float dist_to_light = distance(hit.point, light.position);

        Hit shadowHit;
        for(int i=0; i < NUM_SPHERES; i++) {
            // if(sphere.matID == hit.matID) continue;
            if(intersectSphere(shadowRay, spheres[i], tMin, dist_to_light, shadowHit)){
                diffuse *= 0.2;
                break;
            }
        }
        return hit.color * diffuse;
    } else {
        float a = 0.5f * (normalize(ray.direction).y + 1.0f);
        return (1.0f - a) * float3(1.0f) + a * float3(0.5f, 0.7f, 1.0f);
    }
}

bool intersectSphere(Ray ray, Sphere sphere, float tMin, float tMax, thread Hit& hit) {
    // Goal return true and update all params of hit

    float3 oc = ray.origin - sphere.center;
    float3 dir = ray.direction;
    float radius = sphere.radius;

    float a = dot(dir, dir);
    float b = 2.0f * dot(oc, dir);
    float c = dot(oc, oc) - radius * radius;

    float discriminant = b * b - 4 * a * c;

    if(discriminant < 0) return false;

    float sqrtDiscrim = sqrt(discriminant);

    float t0 = (-b - sqrtDiscrim) / (2.0 * a);
    float t1 = (-b + sqrtDiscrim) / (2.0 * a);

    if(t0 > t1) {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }
    float t = t0;

    if (t < tMin || t > tMax) {
        t = t1;
        if (t < tMin || t > tMax) {
            return false;
        }
    }

    // update hit
    hit.t = t;
    hit.point = ray.origin + t * ray.direction;
    hit.normal = normalize(hit.point - sphere.center);
    if(dot(hit.normal, ray.direction) > 0)
        hit.normal = -hit.normal;
    // hit.matID = matID;
    hit.hit = true;
    hit.color = sphere.color;
    
    return true;
}