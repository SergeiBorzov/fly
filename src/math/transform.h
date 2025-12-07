#ifndef FLY_MATH_TRANSFORM_H
#define FLY_MATH_TRANSFORM_H

#include "mat.h"
#include "quat.h"
#include "vec.h"

namespace Fly
{
namespace Math
{

struct Transform
{
    struct Iterator
    {
        Iterator(Transform* transform) { transform_ = transform; }

        Iterator& operator++()
        {
            if (transform_)
            {
                transform_ = transform_->nextSibling_;
            }
            return *this;
        }

        inline bool operator!=(const Iterator& other) const
        {
            return transform_ != other.transform_;
        }

        Transform& operator*() const { return *transform_; }

    private:
        Transform* transform_;
    };

    struct ConstIterator
    {
        ConstIterator(const Transform* transform) { transform_ = transform; }

        ConstIterator& operator++()
        {
            if (transform_)
            {
                transform_ = transform_->nextSibling_;
            }
            return *this;
        }

        inline bool operator!=(const ConstIterator& other) const
        {
            return transform_ != other.transform_;
        }

        const Transform& operator*() const { return *transform_; }

    private:
        const Transform* transform_;
    };

    Iterator begin() { return Iterator(firstChild_); }
    Iterator end() { return Iterator(nullptr); }
    ConstIterator begin() const { return ConstIterator(firstChild_); }
    ConstIterator end() const { return ConstIterator(nullptr); }

    inline Transform* GetParent() { return parent_; }
    inline const Transform* GetParent() const { return parent_; }
    inline Transform* GetFirstChild() { return firstChild_; }
    inline const Transform* GetFirstChild() const { return firstChild_; }

    void RemoveChild(Transform* child);
    void AddChild(Transform* child);
    void SetParent(Transform* parent);

    Quat GetWorldRotation() const;
    Vec3 GetWorldPosition() const;
    Vec3 GetWorldScale() const;
    inline Mat4 GetWorldMatrix() const { return worldMatrix_; }
    inline Mat4 GetWorldToLocalMatrix() const { return worldToLocalMatrix_; }

    inline Quat GetLocalRotation() const { return localRotation_; }
    inline Vec3 GetLocalPosition() const { return localPosition_; }
    inline Vec3 GetLocalScale() const { return localScale_; }
    Mat4 GetLocalMatrix() const;

    inline Vec3 GetRight() const { return Normalize(Vec3(worldMatrix_[0])); }

    inline Vec3 GetUp() const { return Normalize(Vec3(worldMatrix_[1])); }

    inline Vec3 GetForward() const
    {
        return Normalize(Vec3(worldMatrix_[2]) * -1.0f);
    }

    void SetLocalRotation(Quat quat);
    void SetLocalPosition(Vec3 position);
    void SetLocalScale(Vec3 scale);
    void SetLocalTransform(Vec3 position, Quat rotation = Quat(),
                           Vec3 scale = Vec3(1.0f));

    void SetWorldRotation(Quat rotation);
    void SetWorldPosition(Vec3 position);
    void SetWorldScale(Vec3 scale);
    void SetWorldTransform(Vec3 position, Quat rotation = Quat(),
                           Vec3 scale = Vec3(1.0f));

private:
    void RemoveChildImpl(Transform* child);
    void AddChildImpl(Transform* child);
    void SetParentImpl(Transform* parent);
    void UpdateWorldRecursive();

    Mat4 worldMatrix_ = Mat4(1.0f);
    Mat4 worldToLocalMatrix_ = Mat4(1.0f);

    Quat localRotation_ = Quat();
    Vec3 localPosition_ = Vec3(0.0f);
    Vec3 localScale_ = Vec3(1.0f);

    Transform* parent_ = nullptr;
    Transform* prevSibling_ = nullptr;
    Transform* nextSibling_ = nullptr;
    Transform* firstChild_ = nullptr;
};

} // namespace Math
} // namespace Fly

#endif /* FLY_MATH_TRANSFORM_H */
