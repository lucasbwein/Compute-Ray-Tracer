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

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
Camera debugCam;

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool useDebugCam = false;

bool uiMode = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
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
    // camera.ProcessMouseScroll((float)yoffset);
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
    // glfwSwapInterval(1); // vsync
    glfwSwapInterval(0); // disables vsync
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

    unsigned int rayVBO, rayVAO;
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

// ============ Initialize Metal Render ============
    MetalRenderer metalRenderer;
    metalRenderer.init(SCR_WIDTH, SCR_HEIGHT);

    unsigned int rayTracedTexture = metalRenderer.getOpenGLTextureID();

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

        // processInput(window, camera, deltaTime);
        
        glClear(GL_COLOR_BUFFER_BIT);

// ============ Metal Ray Tracing ============
        metalRenderer.render(activeCam);

        rayShader.use();
        rayShader.setInt("screenTex", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rayTracedTexture);

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

// Ray Tracing Calculation
/* for (int y = 0; y < SCR_HEIGHT; y++){
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
                GL_RGB, GL_FLOAT, colorBuffer.data()); */

// rayShader.use();
// rayShader.setTexture("screenTex", 0);
// glActiveTexture(GL_TEXTURE0);
// glBindTexture(GL_TEXTURE_2D, rayTexture);

// glm::mat4 model = glm::mat4(1.0f);
// glm::mat4 view = glm::mat4(1.0f);
// glm::mat4 projection = glm::mat4(1.0f);

/* view = activeCam.GetViewMatrix();

float aspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
projection = glm::perspective(
    glm::radians(activeCam.Fov),
    aspect,
    0.1f, 100.0f
); */

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