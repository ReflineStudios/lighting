#pragma once

#include <inttypes.h>
#include <bullet/btBulletDynamicsCommon.h>

struct Vec2
{
	float x, y;
};

struct Vec3
{
	float x, y, z;
};

struct Vec4
{
	float x, y, z, w;
};

struct Transform
{
	Vec3 location;
	btQuaternion rotation;
	Vec3 eulerRotation;
	Vec3 scale;
};

#define check(Condition) { if(!(Condition)) __debugbreak(); }

#define d3dcheck(Condition) check(Condition == S_OK)