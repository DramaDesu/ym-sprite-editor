#pragma once

#include <cinttypes>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>

namespace ym::sprite_editor
{
	template<typename T, std::uint8_t N> requires (N > 0)
	struct vec
	{
		vec() = default;

		template<typename... Args>
		vec(Args... args) requires (sizeof...(Args) == N) : data{static_cast<T>(args)...} {}

        T& operator[](std::size_t index) {
            return data[index];
        }

        const T& operator[](std::size_t index) const {
            return data[index];
        }

        T& x() {
            return data[0];
        }

        const T& x() const {
            return data[0];
        }

        T& y() requires (N >= 2) {
            return data[1];
        }

        const T& y() const requires (N >= 2) {
            return data[1];
        }

        T& z() requires (N >= 3) {
            return data[2];
        }

        const T& z() const requires (N >= 3) {
            return data[2];
        }

        T& w() requires (N >= 4) {
            return data[3];
        }

        const T& w() const requires (N >= 4) {
            return data[3];
        }

        vec& operator+=(const vec& rhs) {
            for (std::size_t i = 0; i < N; ++i)
                data[i] += rhs[i];
            return *this;
        }

        vec& operator-=(const vec& rhs) {
            for (std::size_t i = 0; i < N; ++i)
                data[i] -= rhs[i];
            return *this;
        }

        vec& operator*=(const vec& rhs) {
            for (std::size_t i = 0; i < N; ++i)
                data[i] *= rhs[i];
            return *this;
        }

        vec& operator/=(const vec& rhs) {
            for (std::size_t i = 0; i < N; ++i)
                data[i] /= rhs[i];
            return *this;
        }

        bool operator==(const vec& rhs) const requires (!std::is_floating_point_v<T>) {
            for (std::size_t i = 0; i < N; ++i) {
                if (data[i] != rhs[i]) return false;
            }
            return true;
        }

        bool operator!=(const vec& rhs) const requires (!std::is_floating_point_v<T>) {
            return !(*this == rhs);
        }

        bool operator==(const vec& rhs) const requires (std::is_floating_point_v<T>) {
            for (std::size_t i = 0; i < N; ++i) {
                if (std::fabs(data[i] - rhs[i]) > std::numeric_limits<T>::epsilon()) return false;
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
                data[i] += scalar;
            return *this;
        }

        vec& operator-=(T scalar) {
            for (std::size_t i = 0; i < N; ++i)
                data[i] -= scalar;
            return *this;
        }

        vec& operator*=(T scalar) {
            for (std::size_t i = 0; i < N; ++i)
                data[i] *= scalar;
            return *this;
        }

        vec& operator/=(T scalar) {
            for (std::size_t i = 0; i < N; ++i)
                data[i] /= scalar;
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
                result += data[i] * other[i];
            }
            return result;
        }

        T cross(const vec& other) const requires (N == 2 && std::is_floating_point_v<T>) {
            return data[0] * other[1] - data[1] * other[0];
        }

        vec<T, 3> cross(const vec& other) const requires (N == 3 && std::is_floating_point_v<T>) {
            return vec<T, 3> {
                data[1] * other[2] - data[2] * other[1],
                data[2] * other[0] - data[0] * other[2],
                data[0] * other[1] - data[1] * other[0]
            };
        }

        T length() const requires(std::is_floating_point_v<T>) {
            T sum_of_squares = 0;
            for (std::size_t i = 0; i < N; ++i) {
                sum_of_squares += data[i] * data[i];
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
                result[i] = data[i] / len;
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

	private:
		std::array<T, N> data;
	};

	using vec2ui16 = vec<std::uint16_t, 2>;
	using vec2i16 = vec<std::int16_t, 2>;
	using vec2i32 = vec<std::int32_t, 2>;
	using vec2ui32 = vec<std::uint32_t, 2>;
	using vec3 = vec<float, 3>;

    using size_t = vec2ui16;
    using position_t = vec2i32;

    class BaseSprite
    {
    public:
        virtual ~BaseSprite() = default;

        virtual size_t get_size() const = 0;

        position_t position{0, 0};
    };

	class ISpriteEditor
	{
	public:
		virtual ~ISpriteEditor() = default;

        virtual std::shared_ptr<BaseSprite> create_sprite(const position_t& in_position) = 0;

        struct sprite_range
        {
            struct iterator
        	{
                using iterator_category = std::forward_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using value_type = std::shared_ptr<BaseSprite>;
                using pointer = std::shared_ptr<BaseSprite>*;
                using reference = std::shared_ptr<BaseSprite>&;

                std::shared_ptr<BaseSprite> operator*() const;
                iterator& operator++();
                bool operator!=(const iterator& other) const;

                struct iterator_impl
                {
                    virtual ~iterator_impl() = default;
                    virtual std::shared_ptr<BaseSprite> dereference() const = 0;
                    virtual void increment() = 0;
                    virtual bool not_equal(const iterator_impl& other) const = 0;
                };

            private:
                std::unique_ptr<iterator_impl> impl_;

                friend struct sprite_range;
                iterator(std::unique_ptr<iterator_impl> impl) : impl_(std::move(impl)) {}
            };

            iterator begin() const;
            iterator end() const;

            struct range_impl
            {
                virtual ~range_impl() = default;
                virtual std::unique_ptr<iterator::iterator_impl> begin() const = 0;
                virtual std::unique_ptr<iterator::iterator_impl> end() const = 0;
            };

            sprite_range(std::unique_ptr<range_impl> impl) : impl_(std::move(impl)) {}

        private:
            std::unique_ptr<range_impl> impl_;
        };

        virtual sprite_range sprites() const = 0;
        virtual ::size_t sprites_num() const = 0;

        template <typename T, typename Allocator = std::allocator<T>>
        void register_sprite(const Allocator& in_allocator = Allocator())
		{
            on_register_sprite([in_allocator](const position_t& in_position) -> std::shared_ptr<BaseSprite>
            {
                if (auto sprite = std::allocate_shared<T>(in_allocator, in_position)) [[likely]]
                {
                    return std::static_pointer_cast<BaseSprite>(sprite);
                }
                return nullptr;
            });
        }

        using creation_function_t = std::function<std::shared_ptr<BaseSprite>(const position_t& in_position)>;

	protected:
        virtual void on_register_sprite(creation_function_t&& in_sprite_creation) = 0;
	};

    std::shared_ptr<ISpriteEditor> create_sprite_editor();
}
