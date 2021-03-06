#pragma once

#include "types.h"
#include <cmath>

// includes some specialized templates for certain math-like
// algorithms/operations

// returns the lowest of a or b
template<typename T>
[[nodiscard]] constexpr const T &min(const T &a, const T &b)
{
	return a < b ? a : b;
}

// returns the highest of a or b
template<typename T>
[[nodiscard]] constexpr const T &max(const T &a, const T &b)
{
	return a > b ? a : b;
}

// clamp value between min and max (min < value < max)
template<typename T>
[[nodiscard]] constexpr const T &clamp(const T &min, const T &value, const T &max)
{
	return value < min ? min : value > max ? max : value;
}

// a fast method of doing a Euler angular clamp (angle % 360)
[[nodiscard]] constexpr float anglemod(const float &degrees)
{
    return (360.0f / 65536) * ((int32_t)(degrees * (65536 / 360.0f)) & 65535);
}

// pie
constexpr float PI = (float)3.14159265358979323846;

// convert degrees to radians
[[nodiscard]] constexpr float deg2rad(const float &a)
{
	return a * (PI / 180);
}

// convert radians to degrees
[[nodiscard]] constexpr float rad2deg(const float &a)
{
	return a * (180 / PI);
}

// 3d vector is integral to Q2, so it's a global type.
#include "vector.h"