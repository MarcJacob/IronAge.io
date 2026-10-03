// 2D Vector library.

#ifndef VEC_2_INCLUDED
#define VEC_2_INCLUDED

#include "../std_types.h"
#include "numerical.h"

template<typename Numeric>
struct vec2
{
	Numeric x, y;

	constexpr vec2() : x(0), y(0) {}
	constexpr vec2(Numeric in_x, Numeric in_y) : x(in_x), y(in_y) {}

	template<typename OtherNumeric>
	constexpr vec2(const vec2<OtherNumeric>& other) : x((Numeric)other.x), y((Numeric)other.y) {}

	template<typename OtherNumeric>
	constexpr vec2(OtherNumeric in_x, OtherNumeric in_y) : x((Numeric)in_x), y((Numeric)in_y) {}
};

using vec2f = vec2<float>;
using vec2i = vec2<i32>;

// Vec2 - Vec2
template<typename VecTypeA, typename VecTypeB>
static inline vec2<VecTypeA> operator-(const vec2<VecTypeA>& vec_a, vec2<VecTypeB> vec_b)
{
	return { vec_a.x - vec_b.x, vec_a.y - vec_b.y };
}

// Vec2 + Vec2
template<typename VecTypeA, typename VecTypeB>
static inline vec2<VecTypeA> operator+(const vec2<VecTypeA>& vec_a, vec2<VecTypeB> vec_b)
{
	return { vec_a.x + vec_b.x, vec_a.y + vec_b.y };
}

// Vec2 * Scalar
template<typename VecType, typename Scalar>
static inline vec2<VecType> operator*(const vec2<VecType>& vec, Scalar scalar)
{
	return { vec.x * scalar, vec.y * scalar };
}

// Vec2 == Vec2
template<typename VecTypeA, typename VecTypeB>
static inline bool operator==(const vec2<VecTypeA>& vec_a, vec2<VecTypeB> vec_b)
{
	return vec_a.x == vec_b.x && vec_a.y == vec_b.y;
}

template<typename VecAType, typename VecBType>
static inline float vec2_dist_squared(vec2<VecAType> vec_a, vec2<VecBType> vec_b)
{
	return ia_pow(vec_a.x - vec_b.x, 2) + ia_pow(vec_a.y - vec_b.y, 2);
}

template<typename VecAType, typename VecBType>
static inline float vec2_dist(vec2<VecAType> vec_a, vec2<VecBType> vec_b)
{
	float squaredDist = vec2_dist_squared(vec_a, vec_b);
	return ia_sqrt(squaredDist);
}


#endif // VEC_2_INCLUDED