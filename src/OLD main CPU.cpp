#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Model.h"

#include "MetalRenderer.h" // Add renderer header 

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

/* 
---------- Creation Instruction ----------
mkdir build
cmake --build build
cmake --build . && ./OpenGL_Ray
*/

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

// For checking ray hitting
struct Hit {
    bool hit = false;
    float t = 0.0f; // used for how far along until hit
    glm::vec3 point; // where the light hit
    glm::vec3 normal;
    int matID = -1;
};

struct Material {
    glm::vec3 albedo; // used for reflectivity
};

// Spheres
struct Sphere {
    glm::vec3 center;
    float radius;
    int matID;
    bool intersect(const Ray& ray, float tMin, float tMax, Hit& out) const;
};

// used for testing shadows against large plane
struct Plane{
    glm::vec3 point;
    glm::vec3 normal;
    int matID;
    bool intersect(const Ray& ray, float tMin, float tMax, Hit& out) const;
};

struct Color {
    float r, g, b;

    Color(): r(0), g(0), b(0) {}
    Color(glm::vec3 val) : r(val.x), g(val.y), b(val.z) {}
    Color(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}
    glm::vec3 getColor() {
        return glm::vec3(r, g, b);
    }
};

constexpr float INF = 1e30f;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
Camera debugCam;

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool useDebugCam = false;

bool uiMode = false;

std::vector<Sphere> scene;
glm::vec3 lightPos = glm::vec3(1.5f, -2.0f, 2.0f);

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

void processInput(GLFWwindow *window, Camera& camera, float deltaTime){
    static bool fWasPressed = false;
    static bool pWasPressed = false;

    // closes window
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if(glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)  // shows non polygons
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if(glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)  // shows polygons
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)    // moves forwards
        camera.ProcessKeyboardForward(deltaTime);
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)    // moves backwards
        camera.ProcessKeyboardBackward(deltaTime);
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)    // moves left
        camera.ProcessKeyboardLeft(deltaTime);
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)    // moves right
        camera.ProcessKeyboardRight(deltaTime);
    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)// moves up
        camera.ProcessKeyboardUp(deltaTime);
    if(glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)  // moves down
        camera.ProcessKeyboardDown(deltaTime);
    // increases movement speed
    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.Speed += 0.25;
    if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && camera.Speed > 0.25)
        camera.Speed -= 0.25;
    // resets camera, position, and speed
    if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camera.Reset();
        camera.firstMouse = true;
    }
    // Tabs Out of Window (UI switch)
    if(glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
        uiMode = !uiMode;
        glfwSetInputMode(window, GLFW_CURSOR, uiMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        camera.firstMouse = true;
        debugCam.firstMouse = true;
    }

    // Debug Camera Switch
    bool pPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if(pPressed && !pWasPressed) {
        useDebugCam = !useDebugCam;
    }
    pWasPressed = pPressed;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if(uiMode) return;
    Camera &cam = useDebugCam ? debugCam : camera;
    cam.ProcessMouseScroll((float)yoffset);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if(uiMode) return;
    Camera &cam = useDebugCam ? debugCam : camera;
    if (cam.firstMouse)
    {
        cam.lastX = (float)xpos;
        cam.lastY = (float)ypos;
        cam.firstMouse = false;
    }

    float xoffset = (float)xpos - cam.lastX;
    float yoffset = cam.lastY - (float)ypos; // reversed

    cam.lastX = (float)xpos;
    cam.lastY = (float)ypos;

    cam.ProcessMouseMovement(xoffset, yoffset);
}

bool Sphere::intersect(const Ray& ray, float tMin, float tMax, Hit& out) const
{
    /* Idea
    Ray gets compared with object and find d=discriminent (b^2 - 4ac)
    if d > 0: two solutions, d < 0: no solution, d = 0: 1 solution
    Find two  
    */
   

    // oc = o - c
    glm::vec3 oc = ray.origin - center;
    glm::vec3 dir = ray.direction;

    float a = glm::dot(dir, dir);
    float b = 2.0f * glm::dot(oc, dir);
    float c = glm::dot(oc, oc) - radius * radius;
    // can remove the 4 and remove the 2 in below and above equations

    float discriminant = b * b - 4 * a * c;

    if(discriminant < 0) return false;

    float discSqrt = std::sqrt(discriminant);
    float t0 = (-b - discSqrt) / (2 * a);
    float t1 = (-b + discSqrt) / (2 * a);

    if(t0 > t1) std::swap(t0, t1);
    float t = t0;

    if(t < tMin || t > tMax) {
        t = t1;
        if(t < tMin || t > tMax) {
            return false;
        }
    }

    /* 
    Entry point: t0 = -b - sqrt(discriminent) / 2a, first time ray hits sphere
    Exit point: t1 = -b + sqrt(discriminent) / 2a, second intersection on far side
    */
    // Fill hit for referencing
    out.t = t;
    out.point = ray.origin + t * ray.direction;
    out.normal = glm::normalize(out.point - center);
    if(glm::dot(out.normal, ray.direction) > 0) // makes normal face against the incoming ray
        out.normal = -out.normal;
    out.matID = matID;
    out.hit = true;
    return true;
}

// What I need U,V => with FOV => world space => normalize
// Ray(t) = cam.pos + t(dir through pixel)
// Gets the ray from cam based on x and y
void generateRay(int x, int y, Ray& ray) 
{
    float u = 2 * (x + 0.5) / float(SCR_WIDTH) - 1;
    float v = 1 - 2 * (y + 0.5) / float(SCR_HEIGHT);

    float aspect = float(SCR_WIDTH)/float(SCR_HEIGHT);
    float fovRad = glm::radians(camera.Fov);
    float scale = tanf(fovRad * 0.5);

    glm::vec3 dir_cam = 
        camera.Front + 
        (u * aspect * scale) * camera.Right +
        v * scale * camera.Up;

    ray.origin = camera.Position;
    ray.direction = glm::normalize(dir_cam);
}

Color traceRay(const Ray& ray)
{
    Hit hit;
    float tMin = 0.001f;
    float tMax = INF;
    
    for(const Sphere& sphere : scene) { // scene is vector of Sphere that holds objects
        if(sphere.intersect(ray, tMin, tMax, hit)){
            tMax = hit.t; // sets new Max
        }
    }

    if(hit.hit)
    {
        // glm::vec3 lightDir = glm::normalize(hit.point - lightPos);
        glm::vec3 lightDir = glm::normalize(lightPos - hit.point);
        float diffuse = glm::max(glm::dot(hit.normal, lightDir), 0.0f);

        // shadow testing
        Ray shadowRay;
        shadowRay.direction = lightDir;
        shadowRay.origin = hit.point;

        float distToLight = glm::distance(hit.point, lightPos);

        Hit shadowHit;
        for(const Sphere &sphere : scene) {
            // Check if already hit
            if(sphere.matID == hit.matID) continue;
            if(sphere.intersect(shadowRay, 0.001f, distToLight, shadowHit)){
                diffuse *= 0.2;
                break;
            }
        }

        // displays noramls in terms of color
        // return glm::vec3(0.5f * (hit.normal + glm::vec3(1.0f))) * diffuse;
        return glm::vec3(0.0f, 0.0f, 0.8f) * diffuse;
    }
    else
    {
        float a = 0.5f * (glm::normalize(ray.direction).y + 1.0f);
        return (1.0f - a) * glm::vec3(1.0f) *  a * glm::vec3(0.5f, 0.7f, 1.0f);
        // return glm::mix(glm::vec3(1.0f), glm::vec3(0.0, 1.0f, 0.0), a);
    }
        // return background(ray);
}
// bool isInShadow(Hit &hit, Ray &ray, glm::vec3 lightPos) {
//     if(sphere.intersect(ray, 0.0001f))
// }

Color shade(const Hit& hit) {
    return glm::vec3(0.5f * (hit.normal + glm::vec3(1.0f)));
}
Color background(const Ray& ray) {
    return Color(0.0f, 0.0f, 0.0); // black if doesn't hit
    // Typical add sky gradient
    // float t = 0.5f * (ray.direction.y + 1.0f);
    // return (1.0f - t) * Color(1.0f) + t * Color(0.5f, 0.7f, 1.0f);
}

int main() {
    // Calls intialization in the if statement
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    // Request an OpenGL 3.3 core profile context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
// --------------------------
    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    // Make the window's context current
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); // register callback
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
// --------------------------
    // Initializes GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

// --------------------------
    // load shaders
    Shader rayShader("../shaders/ray.vert", "../shaders/ray.frag");
    
    // Shader cubeShader("../shaders/basic.vert", "../shaders/basic.frag");
    // Shader lightingShader("../shaders/lighting.vert", "../shaders/lighting.frag");

    // load models
    // tell stb_image.h to flip loaded texture's on y-axis (before loading model)
    // stbi_set_flip_vertically_on_load(true);
    // Model ourModel("/Users/lucasweinstein/Downloads/backpack/backpack.obj");

    // Model ourModel("/Users/lucasweinstein/Downloads/ImageToStl.com_shiba/shiba.obj");
// --------------------------
    
    Sphere sphere;
    sphere.radius = 0.5f;
    sphere.center = glm::vec3(0.0f, 0.0f, 0.0f);
    sphere.matID = 0;

    Sphere sphere_behind;
    sphere_behind.radius = 1.0f;
    sphere_behind.center = glm::vec3(-0.5f, 2.0f, -1.5f);
    sphere_behind.matID = 1;

    scene.push_back(sphere); // adds the only sphere
    scene.push_back(sphere_behind);

    // scene.push_back();

// ----------------- TEXTURE RAY TRACE MAPPED -----------------
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
        -1.0f, -1.0f,  0.0f, 0.0f,  // bottom-left
        1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right

        -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
        1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
        1.0f,  1.0f,  1.0f, 1.0f   // top-right
    };

    unsigned int rayTexture, rayVBO, rayVAO;
    glGenVertexArrays(1, &rayVAO);
    glGenBuffers(1, &rayVBO);

    glBindVertexArray(rayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rayVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // set VAO -- position layout(location = 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    std::vector<glm::vec3> colorBuffer;
    colorBuffer.resize(SCR_WIDTH * SCR_HEIGHT);

    glGenTextures(1, &rayTexture);
    glBindTexture(GL_TEXTURE_2D, rayTexture);
    // Allocate intial memory
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, 
        SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, nullptr);
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    rayShader.use();
    rayShader.setTexture("screenTex", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rayTexture);

// ============ Initialize Metal Render ============
    // MetalRenderer metalRenderer;
    // metalRenderer.init(SCR_WIDTH, SCR_HEIGHT);

    // unsigned int rayTracedTexture = metalRenderer.getOpenGLTextureID();
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currFrame = glfwGetTime();     // current time
        deltaTime = currFrame - lastFrame;   // time between frames
        lastFrame = currFrame;               // time of last frame

        // FPS => Better Practice is using ImGui Debug
        static int frames = 0;
        static double lastTime = 0.0;
        frames++;
        if (glfwGetTime() - lastTime >= 1){
            double fps = frames / (glfwGetTime() - lastTime);
            frames = 0;
            lastTime = glfwGetTime();

            std::string title = "FPS: " + std::to_string((int)fps);
            glfwSetWindowTitle(window, title.c_str());
        }

        // used in debugging
        if (!useDebugCam) {
            debugCam.Position = camera.Position + glm::vec3(10, 10, 10);
            debugCam.LookAt(camera.Position);
        }
        // gets correct camera to use
        Camera& activeCam = useDebugCam ? debugCam : camera;

        processInput(window, activeCam, deltaTime);

        // Set clear color and clear 
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // background color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // use our shader program and draw first triangle
        // cubeShader.use();
        // cubeShader.setFloat("currFrame", currFrame);

        // cubeShader.setVec3("viewPos", activeCam.Position);

// ============ Metal Ray Tracing ============
        // metalRenderer.render(activeCam);

        // glActiveTexture(GL_TEXTURE0);
        // glBindTexture(GL_TEXTURE_2D, rayTracedTexture);

        // Ray Tracing Calculation
        for (int y = 0; y < SCR_HEIGHT; y++){
            for (int x = 0; x < SCR_WIDTH; x++) {
                Ray ray;
                generateRay(x,y,ray);
                Color color = traceRay(ray);

                int index = y * SCR_WIDTH + x;
                // colorBuffer.resize(SCR_WIDTH * SCR_HEIGHT);
                colorBuffer[index] = color.getColor();
            }
        }
        
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        SCR_WIDTH, SCR_HEIGHT,
                        GL_RGB, GL_FLOAT, colorBuffer.data());

        rayShader.use();
        rayShader.setTexture("screenTex", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rayTexture);


        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);

        view = activeCam.GetViewMatrix();

        float aspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
        projection = glm::perspective(
            glm::radians(activeCam.Fov),
            aspect,
            0.1f, 100.0f
        );

        glBindVertexArray(rayVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // float size = 5.0f; // zoom level

        // projection = glm::ortho(
        //     -size * aspect,  size * aspect,
        //     -size,           size,
        //     0.1f,            100.0f
        // );

        glBindVertexArray(0); 

        // Swap front and back buffers
        glfwSwapBuffers(window);
        // Poll for and process events
        glfwPollEvents();
    }

    // de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &rayVAO);
    // glDeleteBuffers(1, &EBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// Point Light 2
// cubeShader.setVec3("pointLights[1].position", pointLightPositions[1]);
// cubeShader.setFloat("pointLights[1].constant",  1.0f);
// cubeShader.setFloat("pointLights[1].linear",    0.09f);
// cubeShader.setFloat("pointLights[1].quadratic", 0.032f);
// cubeShader.setVec3("pointLights[1].ambient", ambientColor);
// cubeShader.setVec3("pointLights[1].diffuse", diffuseColor);
// cubeShader.setVec3("pointLights[1].specular", 1.0f, 1.0f, 1.0f);
// // Point Light 3
// cubeShader.setVec3("pointLights[2].position", pointLightPositions[2]);
// cubeShader.setFloat("pointLights[2].constant",  1.0f);
// cubeShader.setFloat("pointLights[2].linear",    0.09f);
// cubeShader.setFloat("pointLights[2].quadratic", 0.032f);
// cubeShader.setVec3("pointLights[2].ambient", ambientColor);
// cubeShader.setVec3("pointLights[2].diffuse", diffuseColor);
// cubeShader.setVec3("pointLights[2].specular", 1.0f, 1.0f, 1.0f);
// // Point Light 4
// cubeShader.setVec3("pointLights[3].position", pointLightPositions[3]);
// cubeShader.setFloat("pointLights[3].constant",  1.0f);
// cubeShader.setFloat("pointLights[3].linear",    0.09f);
// cubeShader.setFloat("pointLights[3].quadratic", 0.032f);
// cubeShader.setVec3("pointLights[3].ambient", ambientColor);
// cubeShader.setVec3("pointLights[3].diffuse", diffuseColor);
// cubeShader.setVec3("pointLights[3].specular", 1.0f, 1.0f, 1.0f);

// // Spot Light
// cubeShader.setVec3("spotLight.position", camera.Position);
// cubeShader.setVec3("spotLight.direction", camera.Front);
// cubeShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
// cubeShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
// cubeShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
// cubeShader.setFloat("spotLight.constant", 1.0f);
// cubeShader.setFloat("spotLight.linear", 0.09f);
// cubeShader.setFloat("spotLight.quadratic", 0.032f);
// cubeShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
// cubeShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));
// cubeShader.setInt("spotLight.FlashLightEnable", flashlightOn ? 1 : 0);