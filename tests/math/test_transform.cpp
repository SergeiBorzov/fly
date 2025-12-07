#include <gtest/gtest.h>

#include "core/types.h"
#include "math/transform.h"

using namespace Fly::Math;

static void ExpectVec3Near(Vec3 a, const Vec3 b, float eps = FLY_MATH_EPSILON)
{
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
    EXPECT_NEAR(a.z, b.z, eps);
}

static void ExpectQuatNear(Quat a, Quat b, float eps = FLY_MATH_EPSILON)
{
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
    EXPECT_NEAR(a.z, b.z, eps);
    EXPECT_NEAR(a.w, b.w, eps);
}

static void ExpectMat4Near(const Mat4& a, const Mat4& b,
                           float eps = FLY_MATH_EPSILON)
{
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            EXPECT_NEAR(a[row][col], b[row][col], eps);
        }
    }
}

TEST(TransformTest, DefaultConstructor)
{
    Transform t;

    ExpectVec3Near(t.GetLocalPosition(), Vec3(0.0f));
    ExpectVec3Near(t.GetLocalScale(), Vec3(1.0f));
    ExpectQuatNear(t.GetLocalRotation(), Quat());

    ExpectVec3Near(t.GetWorldPosition(), Vec3(0.0f));
    ExpectVec3Near(t.GetWorldScale(), Vec3(1.0f));
    ExpectQuatNear(t.GetWorldRotation(), Quat());

    ExpectMat4Near(t.GetWorldMatrix(), Mat4(1.0f));
    ExpectMat4Near(t.GetWorldToLocalMatrix(), Mat4(1.0f));

    EXPECT_EQ(t.GetParent(), nullptr);
    EXPECT_EQ(t.GetFirstChild(), nullptr);
}

TEST(TransformTest, AddChildBasic)
{
    Transform parent;
    Transform child;

    parent.AddChild(&child);

    EXPECT_EQ(child.GetParent(), &parent);
    EXPECT_EQ(parent.GetFirstChild(), &child);

    parent.RemoveChild(&child);
    EXPECT_EQ(child.GetParent(), nullptr);
    EXPECT_EQ(parent.GetFirstChild(), nullptr);
}

TEST(TransformTest, SetParentBasic)
{
    Transform parent;
    Transform child;

    child.SetParent(&parent);

    EXPECT_EQ(child.GetParent(), &parent);
    EXPECT_EQ(parent.GetFirstChild(), &child);

    child.SetParent(nullptr);
    EXPECT_EQ(child.GetParent(), nullptr);
    EXPECT_EQ(parent.GetFirstChild(), nullptr);
}

TEST(TransformTest, IteratorTraversal)
{
    Transform root;
    Transform c1, c2, c3;

    root.AddChild(&c1);
    root.AddChild(&c2);
    root.AddChild(&c3);

    u32 count = 0;
    for (Transform& c : root)
    {
        (void)c;
        count++;
    }

    EXPECT_EQ(count, 3);
}

TEST(TransformTest, ConstIteratorTraversal)
{
    Transform root;
    Transform c1, c2, c3, c4;

    root.AddChild(&c1);
    root.AddChild(&c2);
    root.AddChild(&c3);
    root.AddChild(&c4);

    u32 count = 0;
    for (const Transform& c : root)
    {
        (void)c;
        count++;
    }

    EXPECT_EQ(count, 4);
}

TEST(TransformTest, SetLocalPosition)
{
    Transform t;
    t.SetLocalPosition(Vec3(5.0f, 2.0f, -3.0f));

    ExpectVec3Near(t.GetLocalPosition(), Vec3(5.0f, 2.0f, -3.0f));
    ExpectVec3Near(t.GetWorldPosition(), Vec3(5.0f, 2.0f, -3.0f));
    ExpectVec3Near(t.GetLocalScale(), Vec3(1.0f));
    ExpectQuatNear(t.GetLocalRotation(), Quat());
    ExpectVec3Near(t.GetWorldScale(), Vec3(1.0f));
    ExpectQuatNear(t.GetWorldRotation(), Quat());

    f32 values[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 5.0f, 2.0f, -3.0f, 1.0f};
    Mat4 worldMatrix = Mat4(values, 16);
    ExpectMat4Near(t.GetWorldMatrix(), worldMatrix);

    Transform c;
    c.SetParent(&t);
    c.SetLocalPosition(Vec3(0.0f));

    ExpectVec3Near(c.GetLocalPosition(), Vec3(0.0f));
    ExpectVec3Near(c.GetWorldPosition(), Vec3(5.0f, 2.0f, -3.0f));
    ExpectVec3Near(c.GetLocalScale(), Vec3(1.0f));
    ExpectQuatNear(c.GetLocalRotation(), Quat());
    ExpectVec3Near(c.GetWorldScale(), Vec3(1.0f));
    ExpectQuatNear(c.GetWorldRotation(), Quat());
}

// TEST(TransformTest, SetLocalScale)
// {
// }

// TEST(TransformTest, SetLocalRotation)
// {
// }

// TEST(TransformTest, SetLocalTransform)
// {
// }

// TEST(TransformTest, SetWorldPosition)
// {
// }

// TEST(TransformTest, SetWorldRotation)
// {
// }

// TEST(TransformTest, SetWorldScale)
// {
// }

// TEST(TransformTest, GetVectors)
// {
// }

// TEST(TransformTest, LocalMatrixComputed)
// {
// }

// TEST(TransformTest, SetWorldTransform)
// {
// }
