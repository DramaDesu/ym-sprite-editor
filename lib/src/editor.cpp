#include "include/ym-sprite-editor.h"

#include <algorithm>
#include <complex>
#include <corecrt_math_defines.h>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "SDL_render.h"

#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace
{
	constexpr auto tile_size = 8;
	constexpr auto min_tiles_space_size = 2.0f;
	constexpr auto max_tiles_space_size = 8.0f;

	struct FBounds
	{
		FBounds() = default;

		FBounds(float in_size) : min{ -in_size / 2.0f, -in_size / 2.0f }, max{ in_size / 2.0f, in_size / 2.0f } {}

		FBounds(const glm::vec2& in_min, const glm::vec2& in_max)
		{
			min.x = std::min(in_min.x, in_max.x);
			min.y = std::min(in_min.y, in_max.y);
			max.x = std::max(in_min.x, in_max.x);
			max.y = std::max(in_min.y, in_max.y);
		}

		bool Intersects(const FBounds& other) const {
			return !(other.min.x > max.x || other.max.x < min.x || other.min.y > max.y || other.max.y < min.y);
		}

		bool Contains(const glm::vec2& point) const {
			return (point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y);
		}

		void ExpandToFit(const FBounds& other) {
			min.x = std::min(min.x, other.min.x);
			min.y = std::min(min.y, other.min.y);
			max.x = std::max(max.x, other.max.x);
			max.y = std::max(max.y, other.max.y);
		}

		void Offset(const glm::vec2& delta) {
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

		glm::vec2 Size() const {
			return { max.x - min.x, max.y - min.y};
		}

		float Width() const {
			return Size().x;
		}

		float Height() const {
			return Size().y;
		}

		glm::vec2 Left() const {
			return { min.x, 0.0f};
		}

		glm::vec2 Right() const {
			return { max.x, 0.0f};
		}

		glm::vec2 Top() const {
			return { 0.0f, min.y};
		}

		glm::vec2 Bottom() const {
			return { 0.0f, max.y};
		}

		glm::vec2 Center() const {
			return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
		}

		glm::vec2 GetNormalized(const glm::vec2& in_coords) const
		{
			auto&& center = Center();
			auto&& size = Size();

			auto&& origin = in_coords - center;

			return origin / size * 2.0f;
		}

		float ClampX(float in_x) const
		{
			return std::clamp(in_x, min.x, max.x);
		}

		float ClampY(float in_y) const
		{
			return std::clamp(in_y, min.y, max.y);
		}

		glm::vec2 min{};
		glm::vec2 max{};
	};

	struct FCamera
	{
		auto Projection() const -> glm::mat4
		{
			const auto half_size = viewport_bounds.Size() / (2.0f * zoom);
			return glm::ortho(position.x - half_size.x, position.x + half_size.x, position.y - half_size.y, position.y + half_size.y, -1.0f, 1.0f);
		}

		auto WorldSizeToScreenSize(const glm::vec2& in_world_size) const -> glm::vec2
		{
			return in_world_size * zoom;
		}

		auto WorldToScreen(const glm::vec2& in_world_location, const glm::vec2& in_world_size) const -> FBounds
		{
			const auto& screen_location = WorldToScreen(in_world_location);
			const auto& screen_size = WorldSizeToScreenSize(in_world_size);
			return {screen_location - screen_size / 2.0f, screen_location + screen_size / 2.0f};
		}

		auto WorldToScreen(const glm::vec2& in_world_location) const -> glm::vec2
		{
			const auto screen_normalized_location = Projection() * glm::vec4(in_world_location, 0.0f, 1.0f);

			const glm::vec2 normalized_location(screen_normalized_location.x / screen_normalized_location.w,
			                                    screen_normalized_location.y / screen_normalized_location.w);

			const glm::vec2 screen_location = (normalized_location + glm::vec2(1.0f)) * 0.5f * viewport_bounds.Size();

			return viewport_bounds.min + screen_location;
		}


		ImVec2 WorldToScreenImVec(const glm::vec2& in_world_location) const
		{
			const auto& screen_location = WorldToScreen(in_world_location);
			return{ screen_location.x, screen_location.y };
		}

		ym::sprite_editor::vec2 ScreenToWorld(const ym::sprite_editor::vec2& in_screen_pos) const {
			ym::sprite_editor::vec2 world_pos;
			world_pos.x = (in_screen_pos.x - (viewport_bounds.min.x + (viewport_bounds.max.x - viewport_bounds.min.x) / 2)) / zoom + position.x;
			world_pos.y = (in_screen_pos.y - (viewport_bounds.min.y + (viewport_bounds.max.y - viewport_bounds.min.y) / 2)) / zoom + position.y;
			return world_pos;
		}

		ym::sprite_editor::vec2 WorldToMinimap(const ym::sprite_editor::vec2& in_world_pos, const ym::sprite_editor::vec2& in_minimap_pos, const ym::sprite_editor::vec2& in_minimap_size) const {
			const auto minimap_scale_x = 1.0f; // in_minimap_size.x / extends.x;
			const auto minimap_scale_y = 1.0f; // in_minimap_size.y / extends.y;
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

			const auto min_zoom = std::min(viewport_bounds.Width() / (world_extends.x * 2.0f), viewport_bounds.Height() / (world_extends.y * 2.0f));

			return std::clamp(in_zoom, min_zoom, in_max);
			// return std::clamp(in_zoom, in_min, in_max);
		}

		auto ClampLocation(const glm::vec2& in_position) const -> glm::vec2 {
			glm::vec2 out_position;

			const glm::vec2 half_viewport_size = viewport_bounds.Size() / 2.0f / zoom;

			out_position.x = std::clamp(in_position.x, -world_extends.x + half_viewport_size.x, world_extends.x - half_viewport_size.x);
			out_position.y = std::clamp(in_position.y, -world_extends.y + half_viewport_size.y, world_extends.y - half_viewport_size.y);

			return out_position;
		}

		glm::vec2 position{};
		glm::vec2 world_extends{};
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
		glm::vec2 last_mouse_pos{};
	};

	class SegaSprite : public ym::sprite_editor::BaseSprite
	{
	public:
		size_t type() const override { return ym::sprite_editor::types::type_id<SegaSprite>(); }

		glm::vec2 get_size() const override
		{
			return size_ * static_cast<float>(tile_size);
		}

	private:
		glm::vec2 size_{4, 4};
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

		glm::vec2 world_bounds() const override
		{
			return camera.world_extends;
		}

		glm::vec2 world_to_screen(const glm::vec2& in_position) const override
		{
			return camera.WorldToScreen(in_position);
		}

		glm::vec2 world_size_to_screen_size(const glm::vec2& in_world_size) const override
		{
			return camera.WorldSizeToScreenSize(in_world_size);
		}

		static float MinGridSize()
		{
			return static_cast<float>(tile_size) * min_tiles_space_size;
		}

		float MaxGridSize() const {
			const auto max_extend = std::accumulate(sprites_.cbegin(), sprites_.cend(), 0.0f, [](float value, const sprite_t& in_sprite)
			{
				const auto& half_size = in_sprite->get_size() / 2.0f;
				const auto max_x = std::max<float>(std::abs<float>(in_sprite->position.x + half_size.x), std::abs<float>(in_sprite->position.x - half_size.x));
				const auto max_y = std::max<float>(std::abs<float>(in_sprite->position.y + half_size.y), std::abs<float>(in_sprite->position.y - half_size.y));

				return std::max<float>(value, std::max<float>(max_x, max_y));
			});

			return std::max(static_cast<float>(tile_size) * max_tiles_space_size, max_extend);
		}

		void update(const glm::vec2& in_viewport_min, const glm::vec2& in_viewport_max) override
		{
			camera.world_extends = { MaxGridSize(), MaxGridSize() };
			camera.viewport_bounds = { in_viewport_min, in_viewport_max };

			auto&& io = ImGui::GetIO();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
			{
				if (std::abs(io.MouseWheel) > std::numeric_limits<float>::epsilon())
				{
					const float zoom_factor = io.MouseWheel > 0 ? 1.1f : 0.9f;
					const float target_zoom = camera.zoom * zoom_factor;
					zoom.SetTarget(target_zoom);
				}

				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					auto&& mouse_delta = io.MouseDelta;
					camera.position += glm::vec2{ -mouse_delta.x / camera.zoom, -mouse_delta.y / camera.zoom };
				}
				else if (ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					current_selected_sprite.reset();

					for (auto&& sprite : sprites())
					{
						const auto sprite_bounds = camera.WorldToScreen(sprite->position, sprite->get_size());

						const auto mouse_pos = ImGui::GetMousePos();
						if (sprite_bounds.Contains({ mouse_pos.x, mouse_pos.y }))
						{
							select_sprite(sprite);
							ImGui::ClearActiveID();
							break;
						}
					}
				}
			}

			auto&& should_show_mini_map = ImGui::IsMouseHoveringRect({in_viewport_min.x, in_viewport_min.y}, {in_viewport_max.x, in_viewport_max.y});

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

		void draw_sprite_details() const override
		{
			if (auto&& selected_sprite = !current_selected_sprite.expired() ? current_selected_sprite.lock() : nullptr)
			{
				if (auto&& renderer = details_renderers.find(selected_sprite->type()); renderer != details_renderers.cend())
				{
					renderer->second(selected_sprite);
				}
			}
		}

		sprite_range sprites() const override
		{
			struct vector_impl : sprite_range::iterator::iterator_impl
			{
				using it = std::vector<sprite_t>::const_iterator;

				explicit vector_impl(const it& in_begin, const it& in_end) : begin_(in_begin), end_(in_end) {}

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
					return std::make_unique<vector_impl>(sprites_.cbegin(), sprites_.cbegin());
				}
				std::unique_ptr<sprite_range::iterator::iterator_impl> end() const override
				{
					return std::make_unique<vector_impl>(sprites_.cend(), sprites_.cend());
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

		std::weak_ptr<ym::sprite_editor::BaseSprite> selected_sprite() const override
		{
			return current_selected_sprite;
		}

		void select_sprite(const std::shared_ptr<ym::sprite_editor::BaseSprite>& in_sprite) override
		{
			if (auto&& found = std::ranges::find(std::as_const(sprites_), in_sprite); found != sprites_.cend())
			{
				current_selected_sprite = in_sprite;
			}
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

		void on_register_sprite_details_renderer(size_t in_type, renderer_details_function_t&& in_sprite_renderer) override
		{
			details_renderers[in_type] = std::move(in_sprite_renderer);
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

		std::unordered_map<size_t, creation_function_t> creators;
		std::unordered_map<size_t, renderer_function_t> renderers;
		std::unordered_map<size_t, renderer_details_function_t> details_renderers;

		FColorInterpolation mini_map_fade{ {1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 10.5f, EInterpolationType::Sinusoidal };
		FInterpolation zoom;
		FCamera camera;
		FMinimapState minimap_state;
		std::weak_ptr<ym::sprite_editor::BaseSprite> current_selected_sprite;

		std::unique_ptr<drawable_t> drawable_;
	};


	struct sprite_editor_imgui_impl : SegaSpriteEditor::drawable_t
	{
		struct FCursorScreenGuard
		{
			FCursorScreenGuard(const glm::vec2& in_pos)
			{
				ImGui::SetCursorScreenPos({in_pos.x, in_pos.y});
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

				ImGui::SetNextItemAllowOverlap();
				ImGui::InvisibleButton("mini_map", ym::sprite_editor::ToImVec2(minimap_state.screen_bounds.Size()));

				auto&& left_top = ym::sprite_editor::ToImVec2(minimap_state.screen_bounds.min);
				auto&& right_bottom = ym::sprite_editor::ToImVec2(minimap_state.screen_bounds.max);

				draw_list->AddRectFilled(left_top, right_bottom, IM_COL32(50, 50, 50, 255 * alpha));
				{
					auto&& zoom = (camera.viewport_bounds.Size() / 2.0f) / camera.zoom / camera.world_extends;

					auto mini_map_bounds = minimap_state.screen_bounds;
					auto&& world_location = camera.position / camera.world_extends;
					
					mini_map_bounds.Scale(std::max(zoom.x, zoom.y));
					mini_map_bounds.Offset(world_location * minimap_state.screen_bounds.Size() / 2.0f);

					draw_list->AddRect(ym::sprite_editor::ToImVec2(mini_map_bounds.min), ym::sprite_editor::ToImVec2(mini_map_bounds.max), IM_COL32(0, 255, 0, 255 * alpha), 0.0f, 0, 2.0f);
				}
				draw_list->AddRect(left_top, right_bottom, IM_COL32(255, 255, 255, 255 * alpha));

				const glm::vec2 mouse_position = { ImGui::GetMousePos().x, ImGui::GetMousePos().y };

				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					camera.position = minimap_state.screen_bounds.GetNormalized(mouse_position) * camera.world_extends;
					minimap_state.is_dragging = true;
					minimap_state.last_mouse_pos = mouse_position;
				}

				if (minimap_state.is_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					auto&& delta = minimap_state.screen_bounds.GetNormalized(mouse_position) - minimap_state.screen_bounds.GetNormalized(minimap_state.last_mouse_pos);

					camera.position += delta * camera.world_extends;
					minimap_state.last_mouse_pos = mouse_position;
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

		void draw_selected_sprite(ImDrawList* in_draw_list, const std::shared_ptr<ym::sprite_editor::BaseSprite>& in_selected_sprite, const FCamera& in_camera) const
		{
			const auto sprite_bounds = in_camera.WorldToScreen(in_selected_sprite->position, in_selected_sprite->get_size());

			const FCursorScreenGuard guard(sprite_bounds.min);

			const auto mouse_pos = ImGui::GetMousePos();
			const auto is_hovered = sprite_bounds.Contains({ mouse_pos.x, mouse_pos.y });

			ImGui::InvisibleButton("selected_sprite", { sprite_bounds.Size().x, sprite_bounds.Size().y });

			auto&& io = ImGui::GetIO();
			if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				auto&& delta = io.MouseDelta;
				in_selected_sprite->position.x += delta.x / in_camera.zoom;
				in_selected_sprite->position.y += delta.y / in_camera.zoom;
			}

			const ImU32 hatch_color = is_hovered ? IM_COL32(255, 165, 0, 128) : IM_COL32(255, 165, 0, 64); 

			in_draw_list->AddRectFilled({sprite_bounds.min.x, sprite_bounds.min.y}, { sprite_bounds.max.x, sprite_bounds.max.y }, IM_COL32(255, 255, 255, 64));

			in_draw_list->PushClipRect({sprite_bounds.min.x, sprite_bounds.min.y}, { sprite_bounds.max.x, sprite_bounds.max.y }, true);

			constexpr auto hatch_step = 10.0f;
			for (float x = sprite_bounds.min.x; x < sprite_bounds.max.x + sprite_bounds.Size().y; x += hatch_step) {
				in_draw_list->AddLine(ImVec2(x, sprite_bounds.min.y), ImVec2(x - sprite_bounds.Size().y, sprite_bounds.max.y), hatch_color, 2.0f);
			}

			in_draw_list->PopClipRect();

			in_draw_list->AddRect({ sprite_bounds.min.x, sprite_bounds.min.y }, { sprite_bounds.max.x, sprite_bounds.max.y }, hatch_color, 0.0f, 0, 2.0f);
		}

		void draw() const override
		{
			if (editor != nullptr) [[likely]]
			{
				if (auto* draw_list = ImGui::GetWindowDrawList()) [[likely]]
				{
					auto&& camera = editor->camera;

					auto&& left_top = camera.viewport_bounds.min;

					draw_list->AddText({ left_top.x, left_top.y}, IM_COL32(255, 255, 255, 255), std::format("zoom: {}", camera.zoom).c_str());

					draw_list->AddLine(camera.WorldToScreenImVec({-camera.world_extends.x, 0.0f}), camera.WorldToScreenImVec({ camera.world_extends.x, 0.0f }), IM_COL32(255, 255, 255, 255));
					draw_list->AddLine(camera.WorldToScreenImVec({0.0f, -camera.world_extends.y }), camera.WorldToScreenImVec({ 0.0f, camera.world_extends.y }), IM_COL32(255, 255, 255, 255));

					for (auto&& sprite : editor->sprites())
					{
						if (auto&& renderer = editor->renderers.find(sprite->type()); renderer != editor->renderers.cend())
						{
							renderer->second(sprite);
						}
					}

					if (auto&& selected_sprite = !editor->selected_sprite().expired() ? editor->selected_sprite().lock() : nullptr)
					{
						draw_selected_sprite(draw_list, selected_sprite, camera);
					}

					constexpr auto mini_map_coefficient_size = 0.1f; // 10% of viewport
					auto&& viewport_bottom_right = camera.viewport_bounds.max;
					auto&& mini_map_size = glm::vec2{ viewport_bottom_right.x, viewport_bottom_right.x } * mini_map_coefficient_size;
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

	struct item_width_guard
	{
		item_width_guard() : width_(ImGui::CalcItemWidth())
		{
			if (width_ > 0.0f)
			{
				ImGui::PopItemWidth();
			}
		}

		~item_width_guard()
		{
			if (width_ > 0.0f)
			{
				ImGui::PushItemWidth(width_);
			}
		}

	private:
		const float width_;
	};

	void draw_sprite_editor_list(const std::shared_ptr<ym::sprite_editor::ISpriteEditor>& in_sprite_editor)
	{
		const item_width_guard width_guard;

		using namespace std::literals;
		constexpr auto sprite_name = "sprite"sv;
		constexpr auto sprites_in_list = 0x10;

		const ImVec2 list_size = {ImGui::GetFontSize() * sprite_name.size() * 2.0f, ImGui::GetFontSize() * sprites_in_list * 2.0f };

		if (ImGui::BeginListBox("sprites_list", list_size))
		{
			auto sprite_id = 0;
			for (auto&& sprite : in_sprite_editor->sprites())
			{
				const auto is_selected = !in_sprite_editor->selected_sprite().expired() && in_sprite_editor->selected_sprite().lock() == sprite;

				ImGui::PushID(sprite_id++);
				if (ImGui::Selectable("sprite", is_selected))
				{
					in_sprite_editor->select_sprite(sprite);
				}

				if (is_selected) { ImGui::SetItemDefaultFocus(); }

				ImGui::PopID();
			}

			ImGui::EndListBox();
		}
		ImGui::SameLine();
	}

	void draw_sprite_editor_canvas(const std::shared_ptr<ym::sprite_editor::ISpriteEditor>& in_sprite_editor)
	{
		auto&& width = ImGui::CalcItemWidth();
		auto&& position = ImGui::GetCursorScreenPos();

		const ImVec2 viewport_size = { width, width };
		const glm::vec2 viewport_top_left = { position.x, position.y };
		const glm::vec2 viewport_bottom_right = { position.x + width, position.y + width };

		ImGui::SetNextItemAllowOverlap();
		ImGui::InvisibleButton("sprite_editor", viewport_size);

		in_sprite_editor->update(viewport_top_left, viewport_bottom_right);

		if (auto* draw_list = ImGui::GetWindowDrawList())
		{
			ImGui::PushClipRect({viewport_top_left.x, viewport_top_left.y}, {viewport_bottom_right.x, viewport_bottom_right.y}, true);

			in_sprite_editor->draw();

			draw_list->AddQuad(position, { position.x + width, position.y }, { position.x + width, position.y + width }, { position.x, position.y + width }, IM_COL32(128, 128, 128, 255), 3.0f);

			ImGui::PopClipRect();
		}
	}

	void draw_sprite_details(const std::shared_ptr<ym::sprite_editor::ISpriteEditor>& in_sprite_editor)
	{
		in_sprite_editor->draw_sprite_details();
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
			draw_sprite_editor_list(in_sprite_editor);
			draw_sprite_editor_canvas(in_sprite_editor);
			draw_sprite_details(in_sprite_editor);
		}
	}
}