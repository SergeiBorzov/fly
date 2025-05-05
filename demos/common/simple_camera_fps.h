#ifndef HLS_DEMOS_COMMON_SIMPLE_CAMERA_FPS_H
#define HLS_DEMOS_COMMON_SIMPLE_CAMERA_FPS_H

#include "math/mat.h"

struct GLFWwindow;

namespace Hls
{

class SimpleCameraFPS
{
public:
    SimpleCameraFPS(const Math::Mat4& projection, Math::Vec3 position);
    Math::Vec3 GetRightVector();
    Math::Vec3 GetUpVector();
    Math::Vec3 GetForwardVector();
    void SetPosition(Math::Vec3 position);
    inline Math::Vec3 GetPosition() const { return position_; }

    inline const Math::Mat4& GetProjection() const { return projection_; }
    inline const Math::Mat4& GetView() const { return view_; }

    void SetPitch(f32 pitch);
    void Update(GLFWwindow* window, double deltaTime);

private:
    void UpdateRotation(GLFWwindow* window, double deltaTime);
    void UpdatePosition(GLFWwindow* window, double deltaTime);

private:
    Hls::Math::Mat4 projection_;
    Hls::Math::Mat4 view_;
    Hls::Math::Vec3 position_;
    double prevCursorX_ = 0.0;
    double prevCursorY_ = 0.0;
    f32 yaw_ = 90.0;
    f32 pitch_ = 0.0;

public:
    f32 speed = 10.0f;
    f32 sensitivity = 0.1f;

private:
    bool firstFrame_ = true;
};

} // namespace Hls

#endif /* HLS_DEMOS_COMMON_SIMPLE_CAMERA_FPS_H */
