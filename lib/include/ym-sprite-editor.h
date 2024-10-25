#pragma once

#include <cinttypes>
#include <functional>
#include <memory>
#include "imgui.h" // TODO: Move to another header
#include "glm/vec2.hpp"

#include "ym-sprite-editor/math.h"
#include "ym-sprite-editor/types.h"

namespace ym::sprite_editor
{
	class BaseSprite;
	using vec2 = math::vec<float, 2>;
	using vec3 = math::vec<float, 3>;

    template <typename T, typename F>
    concept SpriteCreationCallback = requires(F creation, std::shared_ptr<T> sprite)
    {
        { creation(sprite) } -> std::same_as<void>;
    };

    template <typename T>
    concept IsBaseSprite = std::is_base_of_v<BaseSprite, T>;

    template<typename T, std::uint8_t N>
    ImVec2 ToImVec2(const math::vec<T, N>& in_vector) requires (N >= 2 && std::is_convertible_v<T, float>)
    {
        return { static_cast<float>(in_vector.x), static_cast<float>(in_vector.y) };
    }

    inline ImVec2 ToImVec2(const glm::vec2& in_vector)
    {
        return { static_cast<float>(in_vector.x), static_cast<float>(in_vector.y) };
    }

    class BaseSprite
    {
    public:
        virtual ~BaseSprite() = default;

        virtual size_t type() const = 0;

        virtual glm::vec2 get_size() const = 0;

        glm::vec2 position;
    };

	class ISpriteEditor
	{
	public:
		virtual ~ISpriteEditor() = default;

        virtual void add_sprite(const std::shared_ptr<BaseSprite>& in_sprite) = 0;
        virtual void remove_sprite(const std::shared_ptr<BaseSprite>& in_sprite) = 0;
        virtual std::shared_ptr<BaseSprite> create_sprite() = 0;

        virtual void set_grid_cell_size(std::uint16_t in_size) = 0;

        virtual void update(const glm::vec2& in_viewport_min, const glm::vec2& in_viewport_max) = 0;
		virtual void draw() const = 0;

        virtual void draw_sprite_details() const = 0;

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

        virtual std::weak_ptr<BaseSprite> selected_sprite() const = 0;
        virtual void select_sprite(const std::shared_ptr<BaseSprite>& in_sprite) = 0;
        virtual void focus_camera_on_sprite() = 0;

        virtual glm::vec2 world_bounds() const = 0;

        virtual glm::vec2 world_to_screen(const glm::vec2& in_world_location) const = 0;
        virtual glm::vec2 world_size_to_screen_size(const glm::vec2& in_world_size) const = 0;

        template <typename T> requires IsBaseSprite<T>
        std::shared_ptr<T> create_sprite()
        {
	        return std::static_pointer_cast<T>(on_create_sprite(types::type_id<T>()));
        }

        template <typename T> requires IsBaseSprite<T>
        void default_sprite()
        {
	        on_set_default_sprite(types::type_id<T>());
        }

        template <typename T> requires IsBaseSprite<T>
        static void empty_create_callback(const std::shared_ptr<T>&) {}

        template <typename T, typename F = decltype(&empty_create_callback<T>), typename Allocator = std::allocator<T>>
		requires SpriteCreationCallback<T, F> && IsBaseSprite<T>
        void register_sprite(F&& in_create_callback = empty_create_callback<T>, const Allocator& in_allocator = Allocator())
		{
            on_register_sprite(types::type_id<T>(), [on_created = std::forward<F>(in_create_callback), in_allocator]() -> std::shared_ptr<BaseSprite>
            {
                if (auto sprite = std::allocate_shared<T>(in_allocator)) [[likely]]
                {
                	on_created(sprite);
                    return std::static_pointer_cast<BaseSprite>(sprite);
                }
                return nullptr;
            });
        }

        using creation_function_t = std::function<std::shared_ptr<BaseSprite>()>;
        using renderer_function_t = std::function<void(const std::shared_ptr<BaseSprite>& in_sprite)>;
        using renderer_details_function_t = std::function<void(std::shared_ptr<BaseSprite>& in_sprite)>;

        template <typename T> requires IsBaseSprite<T>
        void register_sprite_renderer(renderer_function_t&& in_sprite_renderer)
        {
            on_register_sprite_renderer(types::type_id<T>(), std::move(in_sprite_renderer));
        }

        template <typename T> requires IsBaseSprite<T>
        void register_sprite_details_renderer(renderer_details_function_t&& in_sprite_renderer)
        {
            on_register_sprite_details_renderer(types::type_id<T>(), std::move(in_sprite_renderer));
        }

	protected:
        virtual void on_set_default_sprite(size_t in_type) = 0;
        virtual void on_register_sprite(size_t in_type, creation_function_t&& in_sprite_creation) = 0;
        virtual void on_register_sprite_renderer(size_t in_type, renderer_function_t&& in_sprite_renderer) = 0;
        virtual void on_register_sprite_details_renderer(size_t in_type, renderer_details_function_t&& in_sprite_renderer) = 0;

        virtual std::shared_ptr<BaseSprite> on_create_sprite(size_t in_type) = 0;
	};

    std::shared_ptr<ISpriteEditor> create_sprite_editor();

    void draw_sprite_editor(const std::shared_ptr<ISpriteEditor>& in_sprite_editor);
}
