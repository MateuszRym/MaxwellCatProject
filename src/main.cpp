// ============================================================================
//  Krecacy sie kot Maxwell - projekt zaliczeniowy z grafiki komputerowej
//  Technologia: C++ / OpenGL 3.3 core, GLFW, GLEW, GLM, Dear ImGui, stb_image
// ============================================================================
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <deque>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// Konfiguracja okna
// ---------------------------------------------------------------------------
static int gWinW = 1280, gWinH = 800;
OrbitCamera gCamera;

bool  gMouseDragging = false;
double gLastMouseX = 0, gLastMouseY = 0;

bool keyState[GLFW_KEY_LAST + 1] = { false };

// ---------------------------------------------------------------------------
// Logika "spinania" kota po spacji
// ---------------------------------------------------------------------------
struct SpinController {
    float spinAngle = 0.0f;      // biezacy kat obrotu kota (Y) w stopniach
    float spinSpeed = 40.0f;     // stopnie/s - predkosc bazowa
    float boost = 0.0f;          // dodatkowy "zapal" z ostatnich wcisniec spacji
    float hype = 0.0f;           // narasta z kazdym wcisnieciem, wlacza disco po przekroczeniu progu
    std::deque<double> pressTimes;

    const float maxBoost = 900.0f;
    const float boostPerPress = 55.0f;
    const float boostDecayPerSec = 35.0f;
    const float hypeDecayPerSec = 4.0f;
    const float hypeThreshold = 100.0f;

    void OnSpacePress(double now) {
        pressTimes.push_back(now);
        while (!pressTimes.empty() && now - pressTimes.front() > 3.0) pressTimes.pop_front();

        // im wiecej wcisniec w krotkim czasie (presses per second), tym wiekszy przyrost
        float pps = (float)pressTimes.size();
        boost = std::min(maxBoost, boost + boostPerPress * (1.0f + pps * 0.12f));
        hype = std::min(200.0f, hype + 6.0f + pps * 0.8f);
    }

    void Update(float dt, bool discoManualOverride) {
        boost = std::max(0.0f, boost - boostDecayPerSec * dt);
        if (!discoManualOverride)
            hype = std::max(0.0f, hype - hypeDecayPerSec * dt);

        float currentSpeed = spinSpeed + boost;
        spinAngle += currentSpeed * dt;
        if (spinAngle > 360.0f) spinAngle -= 360.0f;
    }

    bool DiscoActive() const { return hype >= hypeThreshold; }
    float DiscoBlend() const { return std::clamp((hype - hypeThreshold * 0.5f) / (hypeThreshold * 0.5f), 0.0f, 1.0f); }
    void Reset() { boost = 0; hype = 0; pressTimes.clear(); }
};

SpinController gSpin;

// ---------------------------------------------------------------------------
// Zrodla swiatla
// ---------------------------------------------------------------------------
struct LightGPU {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
};

LightGPU gLight1{ glm::vec3(3.0f, 5.0f, 3.0f), glm::vec3(1.0f, 0.95f, 0.85f), 1.0f }; // "slonce"
LightGPU gLight2{ glm::vec3(-4.0f, 2.5f, -2.0f), glm::vec3(0.4f, 0.6f, 1.0f), 1.0f }; // dopelniajace, chlodne

// swiatla disco - generowane proceduralnie, obracaja sie i zmieniaja kolor
const int kDiscoLightCount = 4;
LightGPU gDiscoLights[kDiscoLightCount];

// ---------------------------------------------------------------------------
// Pomoc: wczytywanie tekstury 2D
// ---------------------------------------------------------------------------
namespace fs = std::filesystem;

static bool FileExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

static std::string ResolveAssetPath(const std::string& relativePath) {
    const fs::path candidates[] = {
        fs::path(relativePath),
        fs::path("..") / relativePath,
        fs::path("..") / ".." / relativePath,
        fs::path(__FILE__).parent_path().parent_path() / relativePath
    };

    for (const auto& candidate : candidates) {
        if (FileExists(candidate)) {
            return candidate.string();
        }
    }

    return relativePath;
}

GLuint LoadTexture(const char* path, bool* ok = nullptr) {
    GLuint tex;
    glGenTextures(1, &tex);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    std::string resolvedPath = ResolveAssetPath(path);
    unsigned char* data = stbi_load(resolvedPath.c_str(), &w, &h, &ch, 0);
    if (data) {
        // use sized internal formats and correct data format; set unpack alignment to avoid row-padding issues
        GLenum internalFormat = (ch == 4) ? GL_RGBA8 : (ch == 3 ? GL_RGB8 : GL_R8);
        GLenum dataFormat = (ch == 4) ? GL_RGBA : (ch == 3 ? GL_RGB : GL_RED);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // if image is single-channel, swizzle it so sampling returns gray in RGB channels
        if (ch == 1) {
            GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        }
        // restore default alignment
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        stbi_image_free(data);
        if (ok) *ok = true;
        std::cout << "Zaladowano teksture: " << resolvedPath << " (" << w << "x" << h << ", " << ch << " kanaly)\n";
    } else {
        std::cerr << "UWAGA: nie znaleziono tekstury '" << path << "' (sprawdzono m.in. '" << resolvedPath << "') - uzyty zostanie kolor zastepczy.\n";
        // teksturka zastepcza 2x2 (szachownica magenta/czarna), zeby brak pliku nie wywalil programu
        unsigned char fallback[16] = {
            255,0,255,255,  0,0,0,255,
            0,0,0,255,      255,0,255,255
        };
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (ok) *ok = false;
    }
    return tex;
}

// ---------------------------------------------------------------------------
// Callbacki GLFW
// ---------------------------------------------------------------------------
void FramebufferSizeCallback(GLFWwindow*, int w, int h) {
    gWinW = w; gWinH = h;
    glViewport(0, 0, w, h);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            gMouseDragging = true;
            glfwGetCursorPos(window, &gLastMouseX, &gLastMouseY);
        } else if (action == GLFW_RELEASE) {
            gMouseDragging = false;
        }
    }
}

void CursorPosCallback(GLFWwindow*, double x, double y) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (gMouseDragging) {
        float dx = (float)(x - gLastMouseX);
        float dy = (float)(y - gLastMouseY);
        gCamera.ProcessMouseDrag(dx, dy);
    }
    gLastMouseX = x; gLastMouseY = y;
}

void ScrollCallback(GLFWwindow*, double, double yoff) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    gCamera.ProcessScroll((float)yoff);
}

void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key >= 0 && key <= GLFW_KEY_LAST) {
        if (action == GLFW_PRESS) keyState[key] = true;
        else if (action == GLFW_RELEASE) keyState[key] = false;
    }
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        gSpin.OnSpacePress(glfwGetTime());
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    if (!glfwInit()) { std::cerr << "Nie udalo sie zainicjalizowac GLFW\n"; return -1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(gWinW, gWinH, "Krecacy sie kot Maxwell - Grafika Komputerowa", nullptr, nullptr);
    if (!window) { std::cerr << "Nie udalo sie utworzyc okna GLFW\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetKeyCallback(window, KeyCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "Nie udalo sie zainicjalizowac GLEW\n"; return -1; }

    glEnable(GL_DEPTH_TEST);

    // ---- ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ---- Shadery ----
    Shader objectShader("shaders/object.vert", "shaders/object.frag");
    Shader bgShader("shaders/background.vert", "shaders/background.frag");

    // ---- Geometria kota (min. 3 niezalezne obiekty: cialo, glowa, ogon [+ uszy]) ----
    Mesh bodyMesh = MakeSphere(1.0f, 32, 24);
    Mesh headMesh = MakeSphere(0.55f, 28, 20);
    Mesh earMesh  = MakeCylinderCone(0.18f, 0.0f, 0.35f, 16);
    Mesh tailMesh = MakeCylinderCone(0.12f, 0.05f, 1.1f, 16);
    Mesh floorMesh = MakePlane(20.0f, 6.0f);
    Mesh discoBallMesh = MakeSphere(0.35f, 20, 16);

    // ---- Quad na pelny ekran (tlo) ----
    float quadVerts[] = { -1,-1,  1,-1,  1,1,  -1,-1, 1,1,  -1,1 };
    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // ---- Tekstury ----
    GLuint furTexture   = LoadTexture("textures/cat_fur.jpg");
    GLuint floorTexture = LoadTexture("textures/disco_floor.jpg");
    GLuint ballTexture  = LoadTexture("textures/disco_ball2.jpg");

    // ---- Parametry konfigurowalne z menu ----
    bool useTextures = true;
    glm::vec3 backgroundColor = glm::vec3(0.35f, 0.42f, 0.36f); // szaro-zielone tlo (klasyczny "green screen" Maxwella)
    float shininess = 48.0f;
    bool manualDiscoOverride = false;
    bool wireframe = false;
    float tailWagSpeed = 4.0f;
    float earTwitchSpeed = 2.5f;

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        dt = std::min(dt, 0.05f); // zabezpieczenie przed duzymi skokami dt

        glfwPollEvents();

        // --- kamera: WASD/QE (gdy nie piszemy w ImGui) ---
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            gCamera.ProcessKeyboard(
                keyState[GLFW_KEY_W], keyState[GLFW_KEY_S],
                keyState[GLFW_KEY_A], keyState[GLFW_KEY_D],
                keyState[GLFW_KEY_Q], keyState[GLFW_KEY_E], dt);
        }

        // --- logika spinu / disco ---
        gSpin.Update(dt, manualDiscoOverride);
        bool discoActive = manualDiscoOverride || gSpin.DiscoActive();
        float discoBlend = manualDiscoOverride ? 1.0f : gSpin.DiscoBlend();

        // --- aktualizacja swiatel disco (obrot + zmiana barwy w czasie) ---
        for (int i = 0; i < kDiscoLightCount; ++i) {
            float phase = (float)i / kDiscoLightCount * 2.0f * glm::pi<float>();
            float angle = (float)now * 1.6f + phase;
            float radius = 4.0f;
            gDiscoLights[i].position = glm::vec3(cosf(angle) * radius, 3.0f + sinf((float)now * 2.0f + phase) * 1.2f, sinf(angle) * radius);
            float hue = fmodf((float)now * 0.25f + (float)i / kDiscoLightCount, 1.0f);
            // prosta konwersja HSV->RGB (S=1, V=1)
            float h6 = hue * 6.0f; int hi = (int)h6; float f = h6 - hi;
            glm::vec3 col;
            switch (hi % 6) {
                case 0: col = {1, f, 0}; break;
                case 1: col = {1 - f, 1, 0}; break;
                case 2: col = {0, 1, f}; break;
                case 3: col = {0, 1 - f, 1}; break;
                case 4: col = {f, 0, 1}; break;
                default: col = {1, 0, 1 - f}; break;
            }
            // zmniejsz lekko intensywnosc czerwieni (zbyt mocny czerwony wyglad kuli disco)
            //if ((hi % 6) == 0 || (hi % 6) == 5) {
           //     col.r = col.r * 0.6f + 0.15f; // zachowaj pewien poziom, ale oslab czerwien
            //}
            // lekka desaturacja calosc dla bardziej zrownowazonego efektu
            col = glm::mix(col, glm::vec3(0.5f), 0.5f);
            gDiscoLights[i].color = col;
            gDiscoLights[i].intensity = 1.0f;
        }

        // ---------------- RENDER ----------------
        glViewport(0, 0, gWinW, gWinH);
        glClear(GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // -- tlo (pelnoekranowy quad, bez testu glebokosci) --
        glDisable(GL_DEPTH_TEST);
        bgShader.Use();
        bgShader.SetFloat("uTime", (float)now);
        bgShader.SetBool("uDiscoMode", discoActive);
        bgShader.SetFloat("uDiscoBlend", discoBlend);
        bgShader.SetVec3("uBaseColor", backgroundColor);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);

        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        glm::mat4 view = gCamera.GetViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(55.0f), (float)gWinW / gWinH, 0.1f, 100.0f);
        glm::vec3 viewPos = gCamera.GetPosition();

        objectShader.Use();
        objectShader.SetMat4("uView", view);
        objectShader.SetMat4("uProjection", proj);
        objectShader.SetVec3("uViewPos", viewPos);
        objectShader.SetFloat("uShininess", shininess);

        objectShader.SetVec3("uLight1.position", gLight1.position);
        objectShader.SetVec3("uLight1.color", gLight1.color);
        objectShader.SetFloat("uLight1.intensity", gLight1.intensity);
        objectShader.SetVec3("uLight2.position", gLight2.position);
        objectShader.SetVec3("uLight2.color", gLight2.color);
        objectShader.SetFloat("uLight2.intensity", gLight2.intensity);

        objectShader.SetBool("uDiscoMode", discoActive);
        objectShader.SetInt("uDiscoLightCount", kDiscoLightCount);
        for (int i = 0; i < kDiscoLightCount; ++i) {
            std::string base = "uDiscoLights[" + std::to_string(i) + "]";
            objectShader.SetVec3(base + ".position", gDiscoLights[i].position);
            objectShader.SetVec3(base + ".color", gDiscoLights[i].color);
            objectShader.SetFloat(base + ".intensity", gDiscoLights[i].intensity);
        }

        auto drawMesh = [&](const Mesh& m, const glm::mat4& model, GLuint tex, const glm::vec3& baseColor) {
            objectShader.SetMat4("uModel", model);
            objectShader.SetMat3("uNormalMatrix" , glm::mat3(glm::transpose(glm::inverse(model))));
            objectShader.SetBool("uUseTexture", useTextures && tex != 0);
            objectShader.SetVec3("uBaseColor", baseColor);
            if (useTextures) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                objectShader.SetInt("uTexture", 0);
            }
            m.Draw();
        };

        // -- podloga --
        glm::mat4 floorModel = glm::mat4(1.0f);
        drawMesh(floorMesh, floorModel, floorTexture, glm::vec3(0.5f));

        // -- kula disco (dodatkowy obiekt, widoczny zawsze, wiruje w rytm klawisza spacja) --
        glm::mat4 ballModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.2f, 0.0f));
        ballModel = glm::rotate(ballModel, glm::radians(gSpin.spinAngle * 1.5f), glm::vec3(0, 1, 0));
        drawMesh(discoBallMesh, ballModel, ballTexture, glm::vec3(0.85f, 0.7f, 0.55f));

        // -- kot Maxwell: 3 (+2) niezalezne obiekty polaczone wspolnym obrotem spinAngle --
        glm::mat4 root = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.1f, 0.0f));
        root = glm::rotate(root, glm::radians(gSpin.spinAngle), glm::vec3(0, 1, 0));

        // cialo
        glm::mat4 bodyModel = root * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 0.85f, 1.1f));
        drawMesh(bodyMesh, bodyModel, furTexture, glm::vec3(0.85f, 0.7f, 0.55f));

        // glowa (przesunieta do przodu/gory wzgledem ciala)
        glm::mat4 headModel = root * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.55f, 0.95f));
        drawMesh(headMesh, headModel, furTexture, glm::vec3(0.85f, 0.7f, 0.55f));

        // uszy - male, niezalezne, delikatnie "drgaja"
        float earTwitch = sinf((float)now * earTwitchSpeed) * 6.0f;
        glm::mat4 headBase = root * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.55f, 0.95f));
        glm::mat4 earL = headBase * glm::translate(glm::mat4(1.0f), glm::vec3(-0.28f, 0.45f, 0.0f));
        earL = glm::rotate(earL, glm::radians(15.0f + earTwitch), glm::vec3(0, 0, 1));
        drawMesh(earMesh, earL, furTexture, glm::vec3(0.85f, 0.7f, 0.55f));

        glm::mat4 earR = headBase * glm::translate(glm::mat4(1.0f), glm::vec3(0.28f, 0.45f, 0.0f));
        earR = glm::rotate(earR, glm::radians(-15.0f - earTwitch), glm::vec3(0, 0, 1));
        drawMesh(earMesh, earR, furTexture, glm::vec3(0.85f, 0.7f, 0.55f));

        // ogon - niezalezny obiekt, macha tym szybciej im wieksza predkosc spinu
        float wagFreq = tailWagSpeed + gSpin.boost * 0.02f;
        float wagAngle = sinf((float)now * wagFreq) * 35.0f;
        glm::mat4 tailModel = root * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.3f, -1.05f));
        tailModel = glm::rotate(tailModel, glm::radians(90.0f + wagAngle), glm::vec3(1, 0, 0));
        drawMesh(tailMesh, tailModel, furTexture, glm::vec3(0.85f, 0.7f, 0.55f));

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // ---------------- MENU KONFIGURACYJNE (ImGui) ----------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Panel konfiguracyjny - Kot Maxwell");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();

        ImGui::Text("Wirowanie kota");
        ImGui::Text("Predkosc spinu: %.1f st/s", gSpin.spinSpeed + gSpin.boost);
        ImGui::Text("Hype: %.0f / %.0f", gSpin.hype, gSpin.hypeThreshold);
        ImGui::ProgressBar(std::min(gSpin.hype / gSpin.hypeThreshold, 1.0f));
        ImGui::SliderFloat("Bazowa predkosc", &gSpin.spinSpeed, 0.0f, 300.0f);
        if (ImGui::Button("Resetuj spin / hype")) gSpin.Reset();
        ImGui::SameLine();
        ImGui::Checkbox("Wymus tryb DISCO", &manualDiscoOverride);

        ImGui::Separator();
        ImGui::Text("Swiatlo 1 (glowne)");
        ImGui::DragFloat3("Pozycja L1", &gLight1.position.x, 0.1f);
        ImGui::ColorEdit3("Kolor L1", &gLight1.color.x);
        ImGui::SliderFloat("Natezenie L1", &gLight1.intensity, 0.0f, 15.0f);

        ImGui::Text("Swiatlo 2 (dopelniajace)");
        ImGui::DragFloat3("Pozycja L2", &gLight2.position.x, 0.1f);
        ImGui::ColorEdit3("Kolor L2", &gLight2.color.x);
        ImGui::SliderFloat("Natezenie L2", &gLight2.intensity, 0.0f, 15.0f);

        ImGui::Separator();
        ImGui::Text("Wyglad / tlo");
        ImGui::ColorEdit3("Kolor tla (bez disco)", &backgroundColor.x);
        ImGui::SliderFloat("Polysk materialu", &shininess, 2.0f, 128.0f);
        ImGui::Checkbox("Uzyj tekstur", &useTextures);
        ImGui::Checkbox("Tryb siatki (wireframe)", &wireframe);
        ImGui::SliderFloat("Predkosc machania ogonem", &tailWagSpeed, 0.5f, 12.0f);
        ImGui::SliderFloat("Predkosc drgania uszu", &earTwitchSpeed, 0.5f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Kamera");
        ImGui::SliderFloat("Dystans", &gCamera.distance, gCamera.minDistance, gCamera.maxDistance);
        ImGui::SliderFloat("Yaw", &gCamera.yaw, -180.0f, 180.0f);
        ImGui::SliderFloat("Pitch", &gCamera.pitch, gCamera.minPitch, gCamera.maxPitch);
        if (ImGui::Button("Resetuj kamere")) { gCamera.distance = 8.0f; gCamera.yaw = -90.0f; gCamera.pitch = 20.0f; }

        ImGui::Separator();
        ImGui::TextWrapped("Sterowanie: SPACJA - kreci kotem (im wiecej/szybciej, tym szybszy spin i szybsze disco), "
                            "mysz (LPM + ruch) - obrot kamery, kolko - zoom, W/S - zoom, A/D - obrot poziomy, Q/E - obrot pionowy, ESC - wyjscie.");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
