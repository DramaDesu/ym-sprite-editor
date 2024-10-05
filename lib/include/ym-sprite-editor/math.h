#pragma once

#include <ostream>
#include <array>

namespace ym::sprite_editor::math
{
	template<typename T, std::uint8_t N> requires (N > 0 && std::is_arithmetic_v<T>)
	struct base_vec
	{
		std::array<T, N> data;
	};

	template<typename T>
	struct base_vec<T, 2>
	{
		union 
		{
			struct { T x, y; };
			struct { T w, h; };
			std::array<T, 2> data;
		};
	};

	template<typename T>
	struct base_vec<T, 3>
	{
		union
		{
			struct { T x, y, z; };
			std::array<T, 3> data;
		};
	};

	template<typename T>
	struct base_vec<T, 4>
	{
		union
		{
			struct { T x, y, z, w; };
			std::array<T, 4> data;
		};
	};

	template<typename T, std::uint8_t N>
	struct vec : base_vec<T, N>
	{
	    vec() = default;

	    template<typename... Args>
	    vec(Args... args) requires (sizeof...(Args) == N) : base_vec<T, N>{ static_cast<T>(args)... } {}

	    T& operator[](std::size_t index) {
	        return this->data[index];
	    }

	    const T& operator[](std::size_t index) const {
	        return this->data[index];
	    }

	    vec& operator+=(const vec& rhs) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] += rhs[i];
	        return *this;
	    }

	    vec& operator-=(const vec& rhs) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] -= rhs[i];
	        return *this;
	    }

	    vec& operator*=(const vec& rhs) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] *= rhs[i];
	        return *this;
	    }

	    vec& operator/=(const vec& rhs) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] /= rhs[i];
	        return *this;
	    }

	    bool operator==(const vec& rhs) const requires (!std::is_floating_point_v<T>) {
	        for (std::size_t i = 0; i < N; ++i) {
	            if (this->data[i] != rhs[i]) return false;
	        }
	        return true;
	    }

	    bool operator!=(const vec& rhs) const requires (!std::is_floating_point_v<T>) {
	        return !(*this == rhs);
	    }

	    bool operator==(const vec& rhs) const requires (std::is_floating_point_v<T>) {
	        for (std::size_t i = 0; i < N; ++i) {
	            if (std::fabs(this->data[i] - rhs[i]) > std::numeric_limits<T>::epsilon()) return false;
	        }
	        return true;
	    }

	    bool operator!=(const vec& rhs) const requires (std::is_floating_point_v<T>) {
	        return !(*this == rhs);
	    }

	    vec operator+(const vec& rhs) const {
	        vec result = *this;
	        result += rhs;
	        return result;
	    }

	    vec operator-(const vec& rhs) const {
	        vec result = *this;
	        result -= rhs;
	        return result;
	    }

	    vec operator*(const vec& rhs) const {
	        vec result = *this;
	        result *= rhs;
	        return result;
	    }

	    vec operator/(const vec& rhs) const {
	        vec result = *this;
	        result /= rhs;
	        return result;
	    }

	    vec& operator+=(T scalar) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] += scalar;
	        return *this;
	    }

	    vec& operator-=(T scalar) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] -= scalar;
	        return *this;
	    }

	    vec& operator*=(T scalar) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] *= scalar;
	        return *this;
	    }

	    vec& operator/=(T scalar) {
	        for (std::size_t i = 0; i < N; ++i)
				this->data[i] /= scalar;
	        return *this;
	    }

	    vec operator+(T scalar) const {
	        vec result = *this;
	        result += scalar;
	        return result;
	    }

	    vec operator-(T scalar) const {
	        vec result = *this;
	        result -= scalar;
	        return result;
	    }

	    vec operator*(T scalar) const {
	        vec result = *this;
	        result *= scalar;
	        return result;
	    }

	    vec operator/(T scalar) const {
	        vec result = *this;
	        result /= scalar;
	        return result;
	    }

	    T dot(const vec& other) const {
	        T result = 0;
	        for (std::size_t i = 0; i < N; ++i) {
	            result += this->data[i] * other[i];
	        }
	        return result;
	    }

	    T cross(const vec& other) const requires (N == 2 && std::is_floating_point_v<T>) {
	        return this->data[0] * other[1] - this->data[1] * other[0];
	    }

	    vec<T, 3> cross(const vec& other) const requires (N == 3 && std::is_floating_point_v<T>) {
	        return vec<T, 3> {
					this->data[1] * other[2] - this->data[2] * other[1],
					this->data[2] * other[0] - this->data[0] * other[2],
					this->data[0] * other[1] - this->data[1] * other[0]
	        };
	    }

	    T length() const requires(std::is_floating_point_v<T>) {
	        T sum_of_squares = 0;
	        for (std::size_t i = 0; i < N; ++i) {
	            sum_of_squares += this->data[i] * this->data[i];
	        }
	        return std::sqrt(sum_of_squares);
	    }

	    vec normalize() const requires(std::is_floating_point_v<T>) {
	        T len = length();
	        if (len == 0) {
	            return {};
	        }

	        vec result;
	        for (std::size_t i = 0; i < N; ++i) {
	            result[i] = this->data[i] / len;
	        }
	        return result;
	    }

	    friend std::ostream& operator<<(std::ostream& os, const vec& v) {
	        os << "{ ";
	        for (std::size_t i = 0; i < N; ++i) {
	            os << v[i];
	            if (i < N - 1) os << ", ";
	        }
	        os << " }";
	        return os;
	    }
	};
}