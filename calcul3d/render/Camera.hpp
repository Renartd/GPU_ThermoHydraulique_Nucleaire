#pragma once
#include <raylib.h>
#include <raymath.h>
#include <cmath>

// Caméra orbitale manuelle — indépendante de UpdateCamera()
struct OrbitalCamera {
    Camera3D cam;

    float yaw      =   0.0f;   // degrés
    float pitch    =  40.0f;   // degrés
    float distance =  20.0f;   // unités monde

    float minDistance =  3.0f;
    float maxDistance = 80.0f;
    float minPitch    = -10.0f;
    float maxPitch    =  89.0f;

    float sensitivity = 0.4f;  // sensibilité souris
    float zoomSpeed   = 1.5f;

    Vector2 lastMouse = {0, 0};

    OrbitalCamera() {
        cam.target     = {0.0f, 0.0f, 0.0f};
        cam.up         = {0.0f, 1.0f, 0.0f};
        cam.fovy       = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        updatePosition();
    }

    void update() {
        Vector2 mouse = GetMousePosition();

        // Rotation : clic gauche OU droit
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
            IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            float dx = mouse.x - lastMouse.x;
            float dy = mouse.y - lastMouse.y;
            yaw   += dx * sensitivity;
            pitch -= dy * sensitivity;
            if (pitch > maxPitch) pitch = maxPitch;
            if (pitch < minPitch) pitch = minPitch;
        }
        lastMouse = mouse;

        // Zoom molette
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            distance -= wheel * zoomSpeed;
            if (distance < minDistance) distance = minDistance;
            if (distance > maxDistance) distance = maxDistance;
        }

        updatePosition();
    }

    Camera3D& get() { return cam; }

private:
    void updatePosition() {
        float yawR   = yaw   * DEG2RAD;
        float pitchR = pitch * DEG2RAD;
        cam.position = {
            cam.target.x + distance * cosf(pitchR) * sinf(yawR),
            cam.target.y + distance * sinf(pitchR),
            cam.target.z + distance * cosf(pitchR) * cosf(yawR)
        };
    }
};
