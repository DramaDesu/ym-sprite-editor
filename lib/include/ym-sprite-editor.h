#pragma once

#include <cinttypes>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>

#include "imgui.h"

#include "ym-sprite-editor/math.h"

namespace ym::sprite_editor
{
	using vec2 = math::vec<float, 2>;
	using vec3 = math::vec<float, 3>;

    using size_t = math::vec<std::uint16_t, 2>;
    using position_t = math::vec<std::int32_t, 2>;

    template<typename T, std::uint8_t N>
    ImVec2 ToImVec2(const ym::sprite_editor::math::vec<T, N>& in_vector) requires (N >= 2 && std::is_convertible_v<T, float>)
    {
        return { in_vector.x, in_vector.y };
    }

    template <typename T>
    std::size_t type_id()
	{
        static const std::size_t id = reinterpret_cast<std::size_t>(&id);
        return id;
    }

    class BaseSprite
    {
    public:
        virtual ~BaseSprite() = default;

        virtual ::size_t type() const = 0;

        virtual size_t get_size() const = 0;

        position_t position{0, 0};
    };

	class ISpriteEditor
	{
	public:
		virtual ~ISpriteEditor() = default;

        virtual std::shared_ptr<BaseSprite> create_sprite(const position_t& in_position) = 0;
        virtual void add_sprite(const std::shared_ptr<BaseSprite>& in_sprite) = 0;

        virtual void update(const vec2& in_viewport_min, const vec2& in_viewport_max) = 0;
		virtual void draw(void* im_context) const = 0;

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

        virtual vec2 world_bounds() const = 0;
        virtual vec2 world_to_screen(const vec2& in_position) const = 0;

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
        using renderer_function_t = std::function<void(const std::shared_ptr<BaseSprite>& in_sprite)>;

        template <typename T>
        void register_sprite_renderer(renderer_function_t&& in_sprite_renderer)
        {
            on_register_sprite_renderer(type_id<T>(), std::move(in_sprite_renderer));
        }

	protected:
        virtual void on_register_sprite(creation_function_t&& in_sprite_creation) = 0;
        virtual void on_register_sprite_renderer(::size_t in_type, renderer_function_t&& in_sprite_renderer) = 0;
	};

    std::shared_ptr<ISpriteEditor> create_sprite_editor();

    void draw_sprite_editor(const std::shared_ptr<ISpriteEditor>& in_sprite_editor);
}
