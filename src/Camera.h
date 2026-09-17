#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

// Kamera orbitalna - obraca się wokół punktu docelowego (kota).
// Sterowanie:
//   - mysz (LPM wcisniety + ruch) -> zmiana yaw/pitch
//   - kolko myszy -> zoom (zmiana promienia orbity)
//   - A / D -> obrot orbity w poziomie (yaw)
//   - Q / E -> obrot orbity w pionie (pitch)
//   - W / S -> przyblizanie / oddalanie (zoom)
class OrbitCamera {
public:
    glm::vec3 target = glm::vec3(0.0f, 1.0f, 0.0f);
    float distance = 8.0f;
    float yaw = -90.0f;   // stopnie
    float pitch = 20.0f;  // stopnie
    float minDistance = 2.5f;
    float maxDistance = 25.0f;
    float minPitch = -80.0f;
    float maxPitch = 85.0f;

    float mouseSensitivity = 0.18f;
    float keyOrbitSpeed = 45.0f;   // stopnie / sekunde (A/D/Q/E)
    float keyZoomSpeed = 6.0f;     // jednostki / sekunde (W/S)
    float scrollSensitivity = 0.6f;

    glm::vec3 GetPosition() const {
        float yawR = glm::radians(yaw);
        float pitchR = glm::radians(pitch);
        glm::vec3 offset;
        offset.x = distance * cos(pitchR) * cos(yawR);
        offset.y = distance * sin(pitchR);
        offset.z = distance * cos(pitchR) * sin(yawR);
        return target + offset;
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(GetPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void ProcessMouseDrag(float dx, float dy) {
        yaw += dx * mouseSensitivity;
        pitch -= dy * mouseSensitivity;
        pitch = std::clamp(pitch, minPitch, maxPitch);
    }

    void ProcessScroll(float yOffset) {
        distance -= yOffset * scrollSensitivity;
        distance = std::clamp(distance, minDistance, maxDistance);
    }

    // wywolywane co klatke na podstawie stanu klawiszy WASD/QE
    void ProcessKeyboard(bool w, bool s, bool a, bool d, bool q, bool e, float dt) {
        if (a) yaw -= keyOrbitSpeed * dt;
        if (d) yaw += keyOrbitSpeed * dt;
        if (q) pitch -= keyOrbitSpeed * dt;
        if (e) pitch += keyOrbitSpeed * dt;
        if (w) distance -= keyZoomSpeed * dt;
        if (s) distance += keyZoomSpeed * dt;
        pitch = std::clamp(pitch, minPitch, maxPitch);
        distance = std::clamp(distance, minDistance, maxDistance);
    }
};
