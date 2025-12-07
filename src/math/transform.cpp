#include "transform.h"

namespace Fly
{
namespace Math
{

Quat Transform::GetWorldRotation() const
{
    Vec3 x = Vec3(worldMatrix_[0]);
    Vec3 y = Vec3(worldMatrix_[1]);
    Vec3 z = Vec3(worldMatrix_[2]);

    float sx = Length(x);
    float sy = Length(y);
    float sz = Length(z);

    if (Math::Abs(sx) < FLY_MATH_EPSILON && Math::Abs(sy) < FLY_MATH_EPSILON &&
        Math::Abs(sz) < FLY_MATH_EPSILON)
    {
        return Quat();
    }

    Mat4 rotMat(1.0f);
    rotMat[0] = Math::Vec4(x / sx, 0.0f);
    rotMat[1] = Math::Vec4(y / sy, 0.0f);
    rotMat[2] = Math::Vec4(z / sz, 0.0f);

    return Quat(rotMat);
}

Vec3 Transform::GetWorldPosition() const { return Vec3(worldMatrix_[3]); }

Vec3 Transform::GetWorldScale() const
{
    return Vec3(Length(Vec3(worldMatrix_[0])), Length(Vec3(worldMatrix_[1])),
                Length(Vec3(worldMatrix_[2])));
}

Mat4 Transform::GetLocalMatrix() const
{
    return TranslationMatrix(localPosition_) * Mat4(localRotation_) *
           ScaleMatrix(localScale_);
}

void Transform::SetLocalPosition(Vec3 position)
{
    localPosition_ = position;
    UpdateWorldRecursive();
}

void Transform::SetLocalRotation(Quat rotation)
{
    localRotation_ = rotation;
    UpdateWorldRecursive();
}

void Transform::SetLocalScale(Vec3 scale)
{
    localScale_ = scale;
    UpdateWorldRecursive();
}

void Transform::SetLocalTransform(Vec3 position, Quat rotation, Vec3 scale)
{
    localPosition_ = position;
    localRotation_ = rotation;
    localScale_ = scale;
    UpdateWorldRecursive();
}

void Transform::SetWorldPosition(Vec3 position)
{
    if (parent_)
    {
        localPosition_ =
            Vec3(parent_->worldToLocalMatrix_ * Vec4(position, 1.0f));
    }
    else
    {
        localPosition_ = position;
    }

    UpdateWorldRecursive();
}

void Transform::SetWorldRotation(Quat rotation)
{
    if (parent_)
    {
        Quat parentRotation = parent_->GetWorldRotation();
        localRotation_ = Inverse(parentRotation) * rotation;
    }
    else
    {
        localRotation_ = rotation;
    }

    UpdateWorldRecursive();
}

void Transform::SetWorldScale(Vec3 scale)
{
    if (parent_)
    {
        Vec3 parentWorldScale = parent_->GetWorldScale();
        localScale_ = scale / parentWorldScale;
    }
    else
    {
        localScale_ = scale;
    }

    UpdateWorldRecursive();
}

void Transform::SetWorldTransform(Vec3 position, Quat rotation, Vec3 scale)
{
    if (parent_)
    {
        Vec3 parentScale = parent_->GetWorldScale();
        Quat parentRotation = parent_->GetWorldRotation();

        localPosition_ =
            Vec3(parent_->worldToLocalMatrix_ * Vec4(position, 1.0f));
        localRotation_ = Inverse(parentRotation) * rotation;
        localScale_ = scale / parentScale;
    }
    else
    {
        localPosition_ = position;
        localRotation_ = rotation;
        localScale_ = scale;
    }

    UpdateWorldRecursive();
}

void Transform::AddChild(Transform* child)
{
    AddChildImpl(child);
    child->UpdateWorldRecursive();
}

void Transform::RemoveChild(Transform* child)
{
    RemoveChildImpl(child);
    child->UpdateWorldRecursive();
}

void Transform::SetParent(Transform* parent)
{
    SetParentImpl(parent);
    UpdateWorldRecursive();
}

void Transform::UpdateWorldRecursive()
{
    if (parent_)
    {
        worldMatrix_ = parent_->worldMatrix_ * GetLocalMatrix();
        worldToLocalMatrix_ = Inverse(worldMatrix_);
    }
    else
    {
        worldMatrix_ = GetLocalMatrix();
        worldToLocalMatrix_ = Inverse(worldMatrix_);
    }

    for (Transform& child : *this)
    {
        child.UpdateWorldRecursive();
    }
}

void Transform::AddChildImpl(Transform* child)
{
    if (!child || child->parent_ == this)
    {
        return;
    }

    if (child->parent_)
    {
        child->parent_->RemoveChild(child);
    }

    if (!firstChild_)
    {
        firstChild_ = child;
        child->prevSibling_ = nullptr;
        child->nextSibling_ = nullptr;
    }
    else
    {
        firstChild_->prevSibling_ = child;
        child->nextSibling_ = firstChild_;
        child->prevSibling_ = nullptr;
        firstChild_ = child;
    }

    child->parent_ = this;
}

void Transform::RemoveChildImpl(Transform* child)
{
    if (!child || child->parent_ != this)
    {
        return;
    }

    if (child->prevSibling_)
    {
        child->prevSibling_->nextSibling_ = child->nextSibling_;
    }
    if (child->nextSibling_)
    {
        child->nextSibling_->prevSibling_ = child->prevSibling_;
    }
    if (firstChild_ == child)
    {
        firstChild_ = child->nextSibling_;
    }

    child->parent_ = nullptr;
    child->prevSibling_ = nullptr;
    child->nextSibling_ = nullptr;
}

void Transform::SetParentImpl(Transform* parent)
{
    if (parent == parent_)
    {
        return;
    }

    if (parent_)
    {
        parent_->RemoveChildImpl(this);
    }

    if (parent)
    {
        parent->AddChildImpl(this);
    }
}

} // namespace Math
} // namespace Fly
