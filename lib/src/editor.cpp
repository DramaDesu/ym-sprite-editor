#include "include/ym-sprite-editor.h"

#include <algorithm>
#include <complex>
#include <corecrt_math_defines.h>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>

#include "imgui.h"
#include "SDL_render.h"

namespace
{
	constexpr auto tile_size = 8;
	constexpr auto min_tiles_space_size = 2.0f;
	constexpr auto max_tiles_space_size = 8.0f;

	struct FBounds
	{
		FBounds() = default;

		FBounds(float in_size) : min{ -in_size / 2.0f, -in_size / 2.0f }, max{ in_size / 2.0f, in_size / 2.0f } {}

		FBounds(const ym::sprite_editor::vec2& in_min, const ym::sprite_editor::vec2& in_max)
			: min(in_min), max(in_max) {}

		bool Intersects(const FBounds& other) const {
			return !(other.min.x > max.x || other.max.x < min.x || other.min.y > max.y || other.max.y < min.y);
		}

		bool Contains(const ym::sprite_editor::vec2& point) const {
			return (point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y);
		}

		void ExpandToFit(const FBounds& other) {
			min.x = std::min(min.x, other.min.x);
			min.y = std::min(min.y, other.min.y);
			max.x = std::max(max.x, other.max.x);
			max.y = std::max(max.y, other.max.y);
		}

		void Offset(const ym::sprite_editor::vec2& delta) {
			min += delta;
			max += delta;
		}

		void Scale(float scale) {
			auto center = Center();
			auto half_size = Size() * 0.5f;

			half_size.x *= scale;
			half_size.y *= scale;

			min = center - half_size;
			max = center + half_size;
		}

		ym::sprite_editor::vec2 Size() const {
			return { max.x - min.x, max.y - min.y};
		}

		float Width() const {
			return Size().x;
		}

		float Height() const {
			return Size().y;
		}

		ym::sprite_editor::vec2 Left() const {
			return { min.x, 0.0f};
		}

		ym::sprite_editor::vec2 Right() const {
			return { max.x, 0.0f};
		}

		ym::sprite_editor::vec2 Top() const {
			return { 0.0f, min.y};
		}

		ym::sprite_editor::vec2 Bottom() const {
			return { 0.0f, max.y};
		}

		ym::sprite_editor::vec2 Center() const {
			return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
		}

		float ClampX(float in_x) const
		{
			return std::clamp(in_x, min.x, max.x);
		}

		float ClampY(float in_y) const
		{
			return std::clamp(in_y, min.y, max.y);
		}

		ym::sprite_editor::vec2 min{};
		ym::sprite_editor::vec2 max{};
	};

	struct FCamera
	{
		ym::sprite_editor::vec2 WorldToScreen(const ym::sprite_editor::vec2& in_world_pos) const
		{
			auto&& screen_position_normalized = (in_world_pos - position) * zoom / extends;
			return viewport_bounds.Center() + (viewport_bounds.Size() / 2.0f) * screen_position_normalized;
		}

		ImVec2 WorldToScreenImVec(const ym::sprite_editor::vec2& in_world_pos) const
		{
			return ym::sprite_editor::ToImVec2(WorldToScreen(in_world_pos));
		}

		ym::sprite_editor::vec2 ScreenToWorld(const ym::sprite_editor::vec2& in_screen_pos) const {
			ym::sprite_editor::vec2 world_pos;
			world_pos.x = (in_screen_pos.x - (viewport_bounds.min.x + (viewport_bounds.max.x - viewport_bounds.min.x) / 2)) / zoom + position.x;
			world_pos.y = (in_screen_pos.y - (viewport_bounds.min.y + (viewport_bounds.max.y - viewport_bounds.min.y) / 2)) / zoom + position.y;
			return world_pos;
		}

		ym::sprite_editor::vec2 WorldToMinimap(const ym::sprite_editor::vec2& in_world_pos, const ym::sprite_editor::vec2& in_minimap_pos, const ym::sprite_editor::vec2& in_minimap_size) const {
			const auto minimap_scale_x = in_minimap_size.x / extends.x;
			const auto minimap_scale_y = in_minimap_size.y / extends.y;
			return {
				in_minimap_pos.x + (in_world_pos.x - position.x) * minimap_scale_x,
				in_minimap_pos.y + (in_world_pos.y - position.y) * minimap_scale_y
			};
		}

		ym::sprite_editor::vec2 MinimapToWorld(const ym::sprite_editor::vec2& minimap_pos, const ym::sprite_editor::vec2& minimap_size, const ym::sprite_editor::vec2& click_pos) const {
			// const auto scale_x = minimap_size.x / world_bounds.Width();
			// const auto scale_y = minimap_size.y / world_bounds.Height();
			// return {
			// 	(click_pos.x - minimap_pos.x) / scale_x + world_bounds.min.x,
			// 	(click_pos.y - minimap_pos.y) / scale_y + world_bounds.min.y
			// };

			return {};
		}

		float ClampZoom(float in_zoom, float in_min, float in_max) const {
			const auto min = std::min(viewport_bounds.Width() / in_min, viewport_bounds.Height() / in_min);
			const auto max = std::min(viewport_bounds.Width() / in_max, viewport_bounds.Height() / in_max);

			// return std::clamp(in_zoom, std::min(min, max), std::max(min, max));
			return std::clamp(in_zoom, in_min, in_max);
		}

		ym::sprite_editor::vec2 ClampLocation(const ym::sprite_editor::vec2& in_position) const {
			auto&& zoomed_extends = extends * std::max(0.0f, zoom - 1.0f);
			ym::sprite_editor::vec2 out_position;
			{
				out_position.x = std::clamp<float>(in_position.x, -zoomed_extends.x, zoomed_extends.x);
				out_position.y = std::clamp<float>(in_position.y, -zoomed_extends.y, zoomed_extends.y);
			}
			return out_position;
		}

		ym::sprite_editor::vec2 position{};
		ym::sprite_editor::vec2 extends{};
		float zoom{ 1.0f };

		FBounds viewport_bounds;
	};

	enum class EInterpolationType {
		Linear,
		QuadraticEaseIn,
		QuadraticEaseOut,
		Sinusoidal
	};

	class FInterpolation {
	public:
		FInterpolation(float in_alpha = 0.0f, float in_speed = 1.05f, EInterpolationType in_interpolation = EInterpolationType::Linear)
			: target_alpha(in_alpha), alpha(in_alpha), speed(in_speed), interpolation(in_interpolation) {}

		float GetAlpha() const { return alpha; }

		void SetTarget(float in_target_alpha) {
			target_alpha = in_target_alpha;
		}

		void Update(float in_delta_time) {
			alpha = Interpolate(alpha, target_alpha, speed * in_delta_time);
		}

	private:
		float Interpolate(float a, float b, float t) const {
			switch (interpolation) {
			case EInterpolationType::Linear:
				return Lerp(a, b, t);
			case EInterpolationType::QuadraticEaseIn:
				return QuadraticEaseIn(a, b, t);
			case EInterpolationType::QuadraticEaseOut:
				return QuadraticEaseOut(a, b, t);
			case EInterpolationType::Sinusoidal:
				return Sinusoidal(a, b, t);
			default:
				return Lerp(a, b, t);
			}
		}

		static float Lerp(float a, float b, float t) {
			return a + (b - a) * t;
		}

		static float QuadraticEaseIn(float a, float b, float t) {
			t = t * t;
			return a + (b - a) * t;
		}

		static float QuadraticEaseOut(float a, float b, float t) {
			t = t * (2.0f - t);
			return a + (b - a) * t;
		}

		static float Sinusoidal(float a, float b, float t) {
			return a + (b - a) * (1 - cosf(t * (M_PI / 2.0f)));
		}

		float target_alpha;
		float alpha;
		float speed;
		EInterpolationType interpolation;
	};

	class FColorInterpolation : public FInterpolation
	{
	public:
		FColorInterpolation(const ImVec4& in_color, float in_alpha = 0.0f, float in_speed = 1.05f, EInterpolationType in_interpolation = EInterpolationType::Linear) :
			FInterpolation(in_alpha, in_speed, in_interpolation), color_(in_color) {}

		ImU32 Color() const {
			const ImVec4 out_color = ImVec4(color_.x, color_.y, color_.z, color_.w * GetAlpha());
			return ImGui::ColorConvertFloat4ToU32(out_color);
		}

	private:
		ImVec4 color_;
	};

	struct FMinimapState
	{
		FBounds screen_bounds{};

		bool is_dragging = false;
		ym::sprite_editor::vec2 last_mouse_pos{};
	};

	class SegaSprite : public ym::sprite_editor::BaseSprite
	{
	public:
		size_t type() const override { return ym::sprite_editor::types::type_id<SegaSprite>(); }

		ym::sprite_editor::size_t get_size() const override
		{
			return size_ * tile_size;
		}

	private:
		ym::sprite_editor::size_t size_{4, 4};
	};

	class SegaSpriteEditor final : public ym::sprite_editor::ISpriteEditor
	{
	public:
		struct drawable_t
		{
			virtual ~drawable_t() = default;
			virtual void target(void* in_target) = 0;
			virtual void draw() const = 0;
		};

		friend struct sprite_editor_imgui_impl;

	public:
		SegaSpriteEditor(std::unique_ptr<drawable_t> in_drawable)
			: zoom(1.0f, tile_size * min_tiles_space_size * 1.5f, EInterpolationType::QuadraticEaseIn)
			, drawable_(std::move(in_drawable))
		{
			drawable_->target(this);
		}

		std::shared_ptr<ym::sprite_editor::BaseSprite> create_sprite() override
		{
			return default_sprite_type.has_value() ? on_create_sprite(default_sprite_type.value()) : nullptr;
		}

		void add_sprite(const std::shared_ptr<ym::sprite_editor::BaseSprite>& in_sprite) override
		{
			sprites_.push_back(in_sprite);
		}

		ym::sprite_editor::vec2 world_bounds() const override
		{
			return camera.extends;
		}

		ym::sprite_editor::vec2 world_to_screen(const ym::sprite_editor::vec2& in_position) const override
		{
			return camera.WorldToScreen(in_position);
		}

		static float MinGridSize()
		{
			return static_cast<float>(tile_size) * min_tiles_space_size;
		}

		float MaxGridSize() const {
			const auto max_extend = std::accumulate(sprites_.cbegin(), sprites_.cend(), 0.0f, [](int value, const sprite_t& in_sprite)
			{
				const auto& half_size = in_sprite->get_size() / 2;
				const auto max_x = std::max<float>(std::abs<float>(in_sprite->position.x + half_size.x), std::abs<float>(in_sprite->position.x - half_size.x));
				const auto max_y = std::max<float>(std::abs<float>(in_sprite->position.y + half_size.y), std::abs<float>(in_sprite->position.y - half_size.y));

				return std::max<float>(value, std::max<float>(max_x, max_y));
			});

			return std::max(static_cast<float>(tile_size) * max_tiles_space_size, max_extend);
		}

		void update(const ym::sprite_editor::vec2& in_viewport_min, const ym::sprite_editor::vec2& in_viewport_max) override
		{
			camera.extends = { MaxGridSize(), MaxGridSize() };
			camera.viewport_bounds = { in_viewport_min, in_viewport_max };

			auto&& io = ImGui::GetIO();
			if (ImGui::IsItemHovered())
			{
				if (io.MouseWheel != 0.0f)
				{
					const float zoom_factor = io.MouseWheel > 0 ? 1.1f : 0.9f;
					const float target_zoom = camera.zoom * zoom_factor;
					zoom.SetTarget(target_zoom);
				}

				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					auto&& mouse_delta = io.MouseDelta;
					camera.position += { -mouse_delta.x / camera.zoom, -mouse_delta.y / camera.zoom };
				}
			}

			auto&& should_show_mini_map = ImGui::IsMouseHoveringRect(ym::sprite_editor::ToImVec2(in_viewport_min), ym::sprite_editor::ToImVec2(in_viewport_max));

			mini_map_fade.SetTarget(should_show_mini_map ? 1.0f : 0.0f);

			zoom.Update(io.DeltaTime);
			mini_map_fade.Update(io.DeltaTime);

			camera.zoom = camera.ClampZoom(zoom.GetAlpha(), 1.0f, MaxGridSize());
			camera.position = camera.ClampLocation(camera.position);
		}

		void draw() const override
		{
			drawable_->draw(); // TODO: Seems like shitty way
		}

		sprite_range sprites() const override
		{
			struct vector_impl : sprite_range::iterator::iterator_impl
			{
				using it = std::vector<sprite_t>::const_iterator;

				explicit vector_impl(it in_begin, it in_end) : begin_(in_begin), end_(in_end) {}

				std::shared_ptr<ym::sprite_editor::BaseSprite> dereference() const override
				{
					return *begin_;
				}
				void increment() override
				{
					++begin_;
				}
				bool not_equal(const iterator_impl& other) const override
				{
					const auto& o = static_cast<const vector_impl&>(other); // TODO
					return begin_ != o.begin_;
				}

			private:
				it begin_;
				it end_;
			};

			struct sprite_range_impl : sprite_range::range_impl
			{
				explicit sprite_range_impl(const std::vector<std::shared_ptr<ym::sprite_editor::BaseSprite>>& sprites)
					: sprites_(sprites) {}

				std::unique_ptr<sprite_range::iterator::iterator_impl> begin() const override
				{
					return std::make_unique<vector_impl>(sprites_.begin(), sprites_.end());
				}
				std::unique_ptr<sprite_range::iterator::iterator_impl> end() const override
				{
					return std::make_unique<vector_impl>(sprites_.end(), sprites_.end());
				}
			private:
				const std::vector<std::shared_ptr<ym::sprite_editor::BaseSprite>>& sprites_;
			};
			return { std::make_unique<sprite_range_impl>(sprites_) };
		}

		size_t sprites_num() const override
		{
			return sprites_.size();
		}

	private:
		void on_set_default_sprite(size_t in_type) override
		{
			default_sprite_type = in_type;
		}

		void on_register_sprite(size_t in_type, creation_function_t&& in_sprite_creation) override
		{
			creators[in_type] = std::move(in_sprite_creation);
		}

		void on_register_sprite_renderer(size_t in_type, renderer_function_t&& in_sprite_renderer) override
		{
			renderers[in_type] = std::move(in_sprite_renderer);
		}

		std::shared_ptr<ym::sprite_editor::BaseSprite> on_create_sprite(::size_t in_type) override
		{
			if (auto it = creators.find(in_type); it != creators.cend())
			{
				auto&& creator = it->second;
				if (auto sprite = creator()) [[likely]]
				{
					sprites_.push_back(sprite);
					return sprite;
				}
			}

			return nullptr;
		}

		using sprite_t = std::shared_ptr<ym::sprite_editor::BaseSprite>;

		std::vector<sprite_t> sprites_;

		std::optional<size_t> default_sprite_type;

		std::unordered_map<::size_t, creation_function_t> creators;
		std::unordered_map<::size_t, renderer_function_t> renderers;

		FColorInterpolation mini_map_fade{ {1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 10.5f, EInterpolationType::Sinusoidal };
		FInterpolation zoom;
		FCamera camera;
		FMinimapState minimap_state;

		std::unique_ptr<drawable_t> drawable_;
	};


	struct sprite_editor_imgui_impl : SegaSpriteEditor::drawable_t
	{
		struct FCursorScreenGuard
		{
			FCursorScreenGuard(const ym::sprite_editor::vec2& in_pos)
			{
				ImGui::SetCursorScreenPos(ym::sprite_editor::ToImVec2(in_pos));
			}

			~FCursorScreenGuard()
			{
				ImGui::SetCursorScreenPos(cached_cursor);
			}
		private:
			ImVec2 cached_cursor = ImGui::GetCursorScreenPos();
		};

		static void draw_minimap(ImDrawList* draw_list, FCamera& camera, FMinimapState& minimap_state, float alpha)
		{
			if (std::abs(alpha) > 0.1f) {
				const FCursorScreenGuard cursor_guard(minimap_state.screen_bounds.min);

				ImGui::InvisibleButton("mini_map", ym::sprite_editor::ToImVec2(minimap_state.screen_bounds.Size()));

				auto&& left_top = ym::sprite_editor::ToImVec2(minimap_state.screen_bounds.min);
				auto&& right_bottom = ym::sprite_editor::ToImVec2(minimap_state.screen_bounds.max);

				draw_list->AddRectFilled(left_top, right_bottom, IM_COL32(50, 50, 50, 255 * alpha));
				{
					auto mini_map_bounds = minimap_state.screen_bounds;
					auto&& world_location = camera.position / camera.extends;
					
					mini_map_bounds.Scale(1.0f / camera.zoom);
					mini_map_bounds.Offset(world_location * mini_map_bounds.Size() / 2.0f);

					draw_list->AddRect(ym::sprite_editor::ToImVec2(mini_map_bounds.min), ym::sprite_editor::ToImVec2(mini_map_bounds.max), IM_COL32(0, 255, 0, 255 * alpha), 0.0f, 0, 2.0f);
				}
				draw_list->AddRect(left_top, right_bottom, IM_COL32(255, 255, 255, 255 * alpha));

				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					ym::sprite_editor::vec2 mouse_pos = {ImGui::GetMousePos().x, ImGui::GetMousePos().y};
					// camera.position = camera.MinimapToWorld(minimap_pos, minimap_size, mouse_pos);
					minimap_state.is_dragging = true;
					minimap_state.last_mouse_pos = mouse_pos;
				}

				if (minimap_state.is_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					ym::sprite_editor::vec2 current_mouse_pos = {ImGui::GetMousePos().x, ImGui::GetMousePos().y};
					ym::sprite_editor::vec2 delta = current_mouse_pos - minimap_state.last_mouse_pos;

					// camera.position += delta;
					minimap_state.last_mouse_pos = current_mouse_pos;
				}

				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					minimap_state.is_dragging = false;
				}
			}
		}

		void target(void* in_source) override
		{
			editor = static_cast<SegaSpriteEditor*>(in_source);
		}

		void draw() const override
		{
			if (editor != nullptr) [[likely]]
			{
				if (auto* draw_list = ImGui::GetWindowDrawList()) [[likely]]
				{
					auto&& camera = editor->camera;

					draw_list->AddLine(camera.WorldToScreenImVec(ym::sprite_editor::vec2{-camera.extends.x, 0.0f} * camera.zoom),
					                   camera.WorldToScreenImVec(ym::sprite_editor::vec2{camera.extends.x, 0.0f } * camera.zoom), IM_COL32(255, 255, 255, 255));
					draw_list->AddLine(camera.WorldToScreenImVec(ym::sprite_editor::vec2{ 0.0f, -camera.extends.y } * camera.zoom),
										camera.WorldToScreenImVec(ym::sprite_editor::vec2{ 0.0f, camera.extends.y } * camera.zoom), IM_COL32(255, 255, 255, 255));

					for (auto&& sprite : editor->sprites())
					{
						if (auto&& renderer = editor->renderers.find(sprite->type()); renderer != editor->renderers.cend())
						{
							renderer->second(sprite);
						}
					}

					constexpr auto mini_map_coefficient_size = 0.1f; // 10% of viewport
					auto&& viewport_bottom_right = camera.viewport_bounds.max;
					auto&& mini_map_size = ym::sprite_editor::vec2{ viewport_bottom_right.x, viewport_bottom_right.x } * mini_map_coefficient_size;
					auto&& mini_map_pos = viewport_bottom_right - mini_map_size - mini_map_size * mini_map_coefficient_size;

					editor->minimap_state.screen_bounds = { mini_map_pos, mini_map_pos + mini_map_size};

					draw_minimap(draw_list, camera, editor->minimap_state, editor->mini_map_fade.GetAlpha());
				}
			}
		}

	private:
		SegaSpriteEditor* editor = nullptr;
	};

	std::shared_ptr<ym::sprite_editor::ISpriteEditor> create_sprite_editor_internal()
	{
		if (auto editor = std::make_shared<SegaSpriteEditor>(std::make_unique<sprite_editor_imgui_impl>())) [[likely]]
		{
			editor->register_sprite<SegaSprite>();
			return editor;
		}
		return nullptr;
	}
}

namespace ym::sprite_editor
{
	std::shared_ptr<BaseSprite> ISpriteEditor::sprite_range::iterator::operator*() const
	{
		return impl_->dereference();
	}

	ISpriteEditor::sprite_range::iterator& ISpriteEditor::sprite_range::iterator::operator++()
	{
		impl_->increment();
		return *this;
	}

	bool ISpriteEditor::sprite_range::iterator::operator!=(const iterator& other) const
	{
		return impl_->not_equal(*other.impl_);
	}

	ISpriteEditor::sprite_range::iterator ISpriteEditor::sprite_range::begin() const
	{
		return impl_->begin();
	}

	ISpriteEditor::sprite_range::iterator ISpriteEditor::sprite_range::end() const
	{
		return impl_->end();
	}

	std::shared_ptr<ISpriteEditor> create_sprite_editor()
	{
		return create_sprite_editor_internal();
	}

	void draw_sprite_editor(const std::shared_ptr<ISpriteEditor>& in_sprite_editor)
	{
		if (in_sprite_editor)
		{
			auto&& width = ImGui::CalcItemWidth();
			auto&& position = ImGui::GetCursorScreenPos();

			const ImVec2 viewport_size = { width, width };
			const vec2 viewport_top_left = { position.x, position.y };
			const vec2 viewport_bottom_right = { position.x + width, position.y + width };

			ImGui::SetNextItemAllowOverlap();
			ImGui::InvisibleButton("sprite_editor", viewport_size);

			in_sprite_editor->update(viewport_top_left, viewport_bottom_right);

			if (auto* draw_list = ImGui::GetWindowDrawList())
			{
				ImGui::PushClipRect(ToImVec2(viewport_top_left), ToImVec2(viewport_bottom_right), true);

				in_sprite_editor->draw();

				draw_list->AddQuad(position, { position.x + width, position.y }, { position.x + width, position.y + width }, { position.x, position.y + width }, IM_COL32(128, 128, 128, 255), 3.0f);

				ImGui::PopClipRect();
			}
		}
	}
}