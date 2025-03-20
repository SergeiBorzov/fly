#include <GLFW/glfw3.h>

#include "simple_camera_fps.h"
#include <stdio.h>

namespace Hls
{

SimpleCameraFPS::SimpleCameraFPS(const Math::Mat4& projection,
                                 Math::Vec3 position)
    : projection_(projection),
      view_(Math::LookAt(position, position + Math::Vec3(0.0f, 0.0f, 1.0f),
                         Math::Vec3(0.0f, 1.0f, 0.0f))),
      position_(position)
{
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
            Normalize(GetRightVector() * moveX + GetUpVector() * moveY +
                      GetForwardVector() * moveZ);
        position_ += moveVector * deltaTime * speed;
    }
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

    double cursorDiffX = cursorX - prevCursorX_;
    double cursorDiffY = cursorY - prevCursorY_;

    prevCursorX_ = cursorX;
    prevCursorY_ = cursorY;

    yaw_ -= cursorDiffX * sensitivity * deltaTime;
    pitch_ -= cursorDiffY * sensitivity * deltaTime;

    pitch_ = Math::Clamp(pitch_, -89.0f, 89.0f);

    Math::Vec3 moveVector = Hls::Math::Vec3(0.0f);

    Math::Vec3 front(
        Math::Cos(Math::Radians(yaw_)) * Math::Cos(Math::Radians(pitch_)),
        Math::Sin(Math::Radians(pitch_)),
        Math::Sin(Math::Radians(yaw_) * Math::Cos(Math::Radians(pitch_))));
    front = Normalize(front);

    view_ = Math::LookAt(position_, position_ + front,
                         Math::Vec3(0.0f, 1.0f, 0.0f));
}

void SimpleCameraFPS::Update(GLFWwindow* window, double deltaTime)
{
    UpdatePosition(window, deltaTime);
    UpdateRotation(window, deltaTime);
}

} // namespace Hls
