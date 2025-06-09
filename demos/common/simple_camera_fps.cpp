#include <GLFW/glfw3.h>

#include "simple_camera_fps.h"

namespace Fly
{

SimpleCameraFPS::SimpleCameraFPS(f32 hFov, f32 aspect, f32 near, f32 far,
                                 Math::Vec3 position)
    : projection_(Math::Perspective(hFov, aspect, near, far)),
      view_(Math::LookAt(position, position + Math::Vec3(0.0f, 0.0f, 1.0f),
                         Math::Vec3(0.0f, 1.0f, 0.0f))),
      position_(position), hFov_(hFov), aspect_(aspect), near_(near), far_(far)
{
    f32 hTanV = Math::Tan(Math::Radians(hFov) * 0.5f) / aspect;
    vFov_ = Math::Degrees(Math::ATan2(hTanV, 1.0f) * 2.0f);
}

Math::Vec3 SimpleCameraFPS::GetRightVector()
{
    return Math::Vec3(view_.data[0], view_.data[4], view_.data[8]);
}

Math::Vec3 SimpleCameraFPS::GetUpVector()
{
    return Math::Vec3(view_.data[1], view_.data[5], view_.data[9]);
}

Math::Vec3 SimpleCameraFPS::GetForwardVector()
{
    return Math::Vec3(view_.data[2], view_.data[6], view_.data[10]);
}

void SimpleCameraFPS::SetPosition(Math::Vec3 position)
{
    position_ = position;
    view_ = Math::LookAt(position, position + GetForwardVector(),
                         Math::Vec3(0.0f, 1.0f, 0.0f));
}

void SimpleCameraFPS::UpdatePosition(GLFWwindow* window, double deltaTime)
{
    i32 moveX = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) -
                (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    i32 moveY = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) -
                (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS);
    i32 moveZ = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) -
                (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);

    if (moveX != 0 || moveY != 0 || moveZ != 0)
    {
        Math::Vec3 moveVector =
            Normalize(GetRightVector() * static_cast<f32>(moveX) +
                      GetUpVector() * static_cast<f32>(moveY) +
                      GetForwardVector() * static_cast<f32>(moveZ));
        position_ += moveVector * static_cast<f32>(deltaTime) * speed;
        view_ = Math::LookAt(position_, position_ + GetForwardVector(),
                             Math::Vec3(0.0f, 1.0f, 0.0f));
    }
}

void SimpleCameraFPS::SetPitch(f32 pitch)
{
    pitch_ = Math::Clamp(pitch, -89.0f, 89.0f);

    Math::Vec3 front(
        Math::Cos(Math::Radians(yaw_)) * Math::Cos(Math::Radians(pitch_)),
        Math::Sin(Math::Radians(pitch_)),
        Math::Sin(Math::Radians(yaw_)) * Math::Cos(Math::Radians(pitch_)));
    front = Normalize(front);

    view_ = Math::LookAt(position_, position_ + front,
                         Math::Vec3(0.0f, 1.0f, 0.0f));
}

void SimpleCameraFPS::UpdateRotation(GLFWwindow* window, double deltaTime)
{
    double cursorX = 0.0f;
    double cursorY = 0.0f;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (firstFrame_)
    {
        prevCursorX_ = cursorX;
        prevCursorY_ = cursorY;
        firstFrame_ = false;
    }

    f32 cursorDiffX = static_cast<f32>(cursorX - prevCursorX_);
    f32 cursorDiffY = static_cast<f32>(cursorY - prevCursorY_);

    prevCursorX_ = cursorX;
    prevCursorY_ = cursorY;

    yaw_ -= cursorDiffX * sensitivity;
    pitch_ -= cursorDiffY * sensitivity;

    pitch_ = Math::Clamp(pitch_, -89.0f, 89.0f);

    Math::Vec3 front(
        Math::Cos(Math::Radians(yaw_)) * Math::Cos(Math::Radians(pitch_)),
        Math::Sin(Math::Radians(pitch_)),
        Math::Sin(Math::Radians(yaw_)) * Math::Cos(Math::Radians(pitch_)));
    front = Normalize(front);

    view_ = Math::LookAt(position_, position_ + front,
                         Math::Vec3(0.0f, 1.0f, 0.0f));
}

void SimpleCameraFPS::Update(GLFWwindow* window, double deltaTime)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        UpdateRotation(window, deltaTime);
        UpdatePosition(window, deltaTime);
    }
    else
    {
        firstFrame_ = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

} // namespace Fly
