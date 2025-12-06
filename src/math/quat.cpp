#include "mat.h"
#include "quat.h"
#include "functions.h"

namespace Fly
{
namespace Math
{

Quat::Quat(const Mat4& mat)
{
    f32 t = mat[0][0] + mat[1][1] + mat[2][2];

    if (t > 0.0f)
    {
        f32 invS = 0.5f / Sqrt(1.0f + t);

        x = (mat[2][1] - mat[1][2]) * invS;
        y = (mat[0][2] - mat[2][0]) * invS;
        z = (mat[1][0] - mat[0][1]) * invS;
        w = 0.25f / invS;
    }
    else
    {
        if (mat[0][0] > mat[1][1] && mat[0][0] > mat[2][2])
        {
            f32 invS =
                0.5f / Sqrt(1.0f + mat[0][0] - mat[1][1] - mat[2][2]);

            x = 0.25f / invS;
            y = (mat[0][1] + mat[1][0]) * invS;
            z = (mat[2][0] + mat[0][2]) * invS;
            w = (mat[2][1] - mat[1][2]) * invS;
        }
        else if (mat[1][1] > mat[2][2])
        {
            f32 invS =
                0.5f / Sqrt(1.0f + mat[1][1] - mat[0][0] - mat[2][2]);

            x = (mat[0][1] + mat[1][0]) * invS;
            y = 0.25f / invS;
            z = (mat[1][2] + mat[2][1]) * invS;
            w = (mat[0][2] - mat[2][0]) * invS;
        }
        else
        {
            f32 invS =
                0.5f / Sqrt(1.0f + mat[2][2] - mat[0][0] - mat[1][1]);

            x = (mat[0][2] + mat[2][0]) * invS;
            y = (mat[1][2] + mat[2][1]) * invS;
            z = 0.25f / invS;
            w = (mat[1][0] - mat[0][1]) * invS;
        }
    }

    *this = Normalize(*this);
}

} // namespace Math
} // namespace Fly
