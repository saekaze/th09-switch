#pragma once
#include "ZunGraphics.hpp"
namespace th09 {
struct GraphicsMath {
    static Vec3 normalize(const Vec3&);
    static Vec3 cross(const Vec3&,const Vec3&);
    static float length(const Vec3&);
    static Matrix4 multiply(const Matrix4&,const Matrix4&);
    static Matrix4 rotation(u32 axis,float angle);
    static Matrix4 look_at(const Vec3& eye,const Vec3& target,const Vec3& up);
    static Matrix4 perspective(float fov,float aspect,float near_plane,float far_plane);
    static Vec3 project(const Vec3&,const Viewport&,const Matrix4& projection,const Matrix4& view,const Matrix4& world);
};
}
