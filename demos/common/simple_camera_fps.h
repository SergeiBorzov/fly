#ifndef FLY_DEMOS_COMMON_SIMPLE_CAMERA_FPS_H
#define FLY_DEMOS_COMMON_SIMPLE_CAMERA_FPS_H

#include "math/mat.h"

struct GLFWwindow;

namespace Fly
{

class SimpleCameraFPS
{
public:
    SimpleCameraFPS(f32 hFov, f32 aspect, f32 near, f32 far,
                    Math::Vec3 position);
    Math::Vec3 GetRightVector();
    Math::Vec3 GetUpVector();
    Math::Vec3 GetForwardVector();
    void SetPosition(Math::Vec3 position);
    inline Math::Vec3 GetPosition() const { return position_; }

    inline const Math::Mat4& GetProjection() const { return projection_; }
    inline const Math::Mat4& GetView() const { return view_; }

    inline f32 GetVerticalFov() const { return vFov_; }
    inline f32 GetHorizontalFov() const { return hFov_; }
    inline f32 GetAspect() const { return aspect_; }
    inline f32 GetNear() const { return near_; }
    inline f32 GetFar() const { return far_; }

    void SetPitch(f32 pitch);
    void Update(GLFWwindow* window, double deltaTime);

private:
    void UpdateRotation(GLFWwindow* window, double deltaTime);
    void UpdatePosition(GLFWwindow* window, double deltaTime);

private:
    Fly::Math::Mat4 projection_;
    Fly::Math::Mat4 view_;
    Fly::Math::Vec3 position_;
    double prevCursorX_ = 0.0;
    double prevCursorY_ = 0.0;
    f32 yaw_ = 0.0f;
    f32 pitch_ = 0.0f;
    f32 vFov_;
    f32 hFov_;
    f32 aspect_;
    f32 near_;
    f32 far_;

public:
    f32 speed = 10.0f;
    f32 sensitivity = 0.1f;

private:
    bool firstFrame_ = true;
};

} // namespace Fly

#endif /* FLY_DEMOS_COMMON_SIMPLE_CAMERA_FPS_H */
