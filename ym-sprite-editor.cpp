#include "lib/include/ym-sprite-editor.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#define SDL_MAIN_HANDLED
#include <filesystem>

#include "SDL.h"

#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

#include <cmath>
#include <iostream>

using namespace std;

namespace ym::ui {

	enum class EInterpolationType {
		Linear,
		QuadraticEaseIn,
		QuadraticEaseOut,
		Sinusoidal
	};

	ImVec2 operator-(const ImVec2& in_vector)
	{
		return {-in_vector.x, -in_vector.y};
	}

	ImVec2 operator+(const ImVec2& in_vector, float in_value)
	{
		return { in_vector.x + in_value, in_vector.y + in_value };
	}

	ImVec2 operator*(const ImVec2& in_vector, float in_value)
	{
		return { in_vector.x * in_value, in_vector.y * in_value };
	}

	ImVec2 operator/(const ImVec2& in_vector, float in_value)
	{
		return { in_vector.x / in_value, in_vector.y / in_value };
	}

	ImVec2 operator+(const ImVec2& in_vector_a, const ImVec2& in_vector_b)
	{
		return { in_vector_a.x + in_vector_b.x, in_vector_a.y + in_vector_b.y };
	}

	ImVec2 operator-(const ImVec2& in_vector_a, const ImVec2& in_vector_b)
	{
		return { in_vector_a.x - in_vector_b.x, in_vector_a.y - in_vector_b.y };
	}

	ImVec2 operator/(const ImVec2& in_vector_a, const ImVec2& in_vector_b)
	{
		return { in_vector_a.x / in_vector_b.x, in_vector_a.y / in_vector_b.y };
	}

	ImVec2& operator+=(ImVec2& in_vector_a, const ImVec2& in_vector_b)
	{
		in_vector_a.x += in_vector_b.x;
		in_vector_a.y += in_vector_b.y;
		return in_vector_a;
	}

	ImVec2& operator-=(ImVec2& in_vector_a, const ImVec2& in_vector_b)
	{
		in_vector_a.x -= in_vector_b.x;
		in_vector_a.y -= in_vector_b.y;
		return in_vector_a;
	}

	struct FBounds
	{
		FBounds() = default;

		FBounds(float in_size) : min{-in_size / 2.0f, -in_size / 2.0f }, max{ in_size / 2.0f, in_size / 2.0f } {}

		FBounds(const ImVec2& in_min, const ImVec2& in_max)
			: min(in_min), max(in_max) {}

		bool Intersects(const FBounds& other) const {
			return !(other.min.x > max.x || other.max.x < min.x || other.min.y > max.y || other.max.y < min.y);
		}

		bool Contains(const ImVec2& point) const {
			return (point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y);
		}

		void ExpandToFit(const FBounds& other) {
			min.x = std::min(min.x, other.min.x);
			min.y = std::min(min.y, other.min.y);
			max.x = std::max(max.x, other.max.x);
			max.y = std::max(max.y, other.max.y);
		}

		void Offset(const ImVec2& delta) {
			min.x += delta.x;
			min.y += delta.y;
			max.x += delta.x;
			max.y += delta.y;
		}

		void Scale(float scale) {
			ImVec2 center = Center();
			ImVec2 half_size = Size() * 0.5f;

			half_size.x *= scale;
			half_size.y *= scale;

			min = center - half_size;
			max = center + half_size;
		}

		ImVec2 Size() const {
			return {max.x - min.x, max.y - min.y};
		}

		float Width() const {
			return Size().x;
		}

		float Height() const {
			return Size().y;
		}

		ImVec2 Left() const {
			return { min.x, 0.0f };
		}

		ImVec2 Right() const {
			return { max.x, 0.0f };
		}

		ImVec2 Top() const {
			return { 0.0f, min.y };
		}

		ImVec2 Bottom() const {
			return { 0.0f, max.y };
		}

		ImVec2 Center() const {
			return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
		}

		float ClampX(float in_x) const
		{
			return std::clamp(in_x, min.x, max.x);
		}

		float ClampY(float in_y) const
		{
			return std::clamp(in_y, min.y, max.y);
		}

		ImVec2 min{};
		ImVec2 max{};
	};

	struct FCamera
	{
		FBounds VisibleWorldBounds() const {
			const auto half_viewport_size = ImVec2(viewport_bounds.Width(), viewport_bounds.Height()) / (2.0f * zoom);
			return { position - half_viewport_size, position + half_viewport_size };
		}

		ImVec2 WorldToScreen(const ImVec2& in_world_pos) const {
			ImVec2 screen_pos;
			screen_pos.x = (in_world_pos.x - position.x) * zoom + (viewport_bounds.min.x + (viewport_bounds.max.x - viewport_bounds.min.x) / 2);
			screen_pos.y = (in_world_pos.y - position.y) * zoom + (viewport_bounds.min.y + (viewport_bounds.max.y - viewport_bounds.min.y) / 2);
			return screen_pos;
		}

		ImVec2 ScreenToWorld(const ImVec2& in_screen_pos) const {
			ImVec2 world_pos;
			world_pos.x = (in_screen_pos.x - (viewport_bounds.min.x + (viewport_bounds.max.x - viewport_bounds.min.x) / 2)) / zoom + position.x;
			world_pos.y = (in_screen_pos.y - (viewport_bounds.min.y + (viewport_bounds.max.y - viewport_bounds.min.y) / 2)) / zoom + position.y;
			return world_pos;
		}

		ImVec2 WorldToMinimap(const ImVec2& in_world_pos, const ImVec2& in_minimap_pos, const ImVec2& in_minimap_size) const {
			const auto minimap_scale_x = in_minimap_size.x / world_bounds.Width();
			const auto minimap_scale_y = in_minimap_size.y / world_bounds.Height();
			return {
				in_minimap_pos.x + (in_world_pos.x - world_bounds.min.x) * minimap_scale_x,
				in_minimap_pos.y + (in_world_pos.y - world_bounds.min.y) * minimap_scale_y
			};
		}

		ImVec2 MinimapToWorld(const ImVec2& minimap_pos, const ImVec2& minimap_size, const ImVec2& click_pos) const {
			const auto scale_x = minimap_size.x / world_bounds.Width();
			const auto scale_y = minimap_size.y / world_bounds.Height();
			return {
				(click_pos.x - minimap_pos.x) / scale_x + world_bounds.min.x,
				(click_pos.y - minimap_pos.y) / scale_y + world_bounds.min.y
			};
		}

		float ClampZoom(float in_zoom, float in_min, float in_max) const {
			const auto min = std::min(viewport_bounds.Width() / in_min, viewport_bounds.Height() / in_min);
			const auto max = std::min(viewport_bounds.Width() / in_max, viewport_bounds.Height() / in_max);

			return std::clamp(in_zoom, std::min(min, max), std::max(min, max));
		}

		ImVec2 ClampLocation(const ImVec2& in_position) const {
			const auto viewport_width = viewport_bounds.Width() / zoom;
			const auto viewport_height = viewport_bounds.Height() / zoom;
			ImVec2 out_position; {
				out_position.x = std::clamp(in_position.x, world_bounds.min.x + viewport_width / 2, world_bounds.max.x - viewport_width / 2);
				out_position.y = std::clamp(in_position.y, world_bounds.min.y + viewport_height / 2, world_bounds.max.y - viewport_height / 2);
			}
			return out_position;
		}

		ImVec2 position{};
		FBounds world_bounds;
		float zoom{1.0f};
		FBounds viewport_bounds;
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

	void draw_grid(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right, float grid_size, ImU32 color, float scale) {
		float scaled_grid_size = grid_size * scale;

		for (float x = top_left.x; x < bottom_right.x; x += scaled_grid_size) {
			draw_list->AddLine(ImVec2(x, top_left.y), ImVec2(x, bottom_right.y), color);
		}

		for (float y = top_left.y; y < bottom_right.y; y += scaled_grid_size) {
			draw_list->AddLine(ImVec2(top_left.x, y), ImVec2(bottom_right.x, y), color);
		}
	}

	void draw_grid_with_camera(ImDrawList* draw_list, const FCamera& camera, float grid_size, ImU32 color, ImVec2 cursor_screen_pos)
	{
		auto&& origin_min = camera.WorldToScreen({-grid_size, -grid_size });
		auto&& origin_max = camera.WorldToScreen({ grid_size, grid_size });

		draw_list->AddLine(camera.WorldToScreen(camera.world_bounds.Left()), camera.WorldToScreen(camera.world_bounds.Right()), IM_COL32(255, 255, 255, 255));
		draw_list->AddLine(camera.WorldToScreen(camera.world_bounds.Top()), camera.WorldToScreen(camera.world_bounds.Bottom()), IM_COL32(255, 255, 255, 255));

		// draw_list->AddRectFilled(origin_min, origin_max, color);
	}

	void draw_rect_with_camera(ImDrawList* draw_list, const FCamera& camera, float size, ImU32 color)
	{
		auto&& origin_min = camera.WorldToScreen({ -size, -size });
		auto&& origin_max = camera.WorldToScreen({ size, size });

		draw_list->AddRectFilled(origin_min, origin_max, color);
	}

	struct FMinimapState
	{
		bool is_dragging = false;
		ImVec2 last_mouse_pos{};
	};

	class FTexture
	{
	public:
		~FTexture()
		{
			Free();
		}

		void Load(std::uint8_t* in_data, size_t in_depth, int in_width, int in_height, SDL_Renderer* in_renderer)
		{
			Free();

			if (SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(in_data, in_width, in_height, in_depth * 8, in_depth * in_width,
			0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000)) {
				texture_ = SDL_CreateTextureFromSurface(in_renderer, surface);
				width_ = in_width;
				height_ = in_height;
				SDL_FreeSurface(surface);
			}
		}

		operator bool() const {
			return texture_ != nullptr;
		}

		SDL_Texture* get_texture() const { return texture_; }
		float get_width() const { return width_; }
		float get_height() const { return height_; }

	private:
		void Free()
		{
			SDL_DestroyTexture(texture_);
			texture_ = nullptr;
		}

		SDL_Texture* texture_ = nullptr;
		float width_ = 0.0f;
		float height_ = 0.0f;
	};

	struct FSpriteEditorViewModel {
		FSpriteEditorViewModel(std::uint16_t in_tile_size = 0x8, float in_min_tiles_space_size = 2.0f, float in_tiles_space_size = 8.0f)
			: tile_size(in_tile_size),
			  min_tiles_space_size(in_min_tiles_space_size),
			  tiles_space_size(in_tiles_space_size),
			  zoom(1.0f, static_cast<float>(in_tile_size) * in_min_tiles_space_size * 1.5f, EInterpolationType::QuadraticEaseIn)
		{
			camera.world_bounds = MaxGridSize();
		}
			
		void Update(const ImVec2& in_viewport_top_left, const ImVec2& in_viewport_bottom_right) {
			camera.viewport_bounds = {in_viewport_top_left, in_viewport_bottom_right};

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

			auto&& should_show_mini_map = ImGui::IsMouseHoveringRect(in_viewport_top_left, in_viewport_bottom_right);

			mini_map_fade.SetTarget(should_show_mini_map ? 1.0f : 0.0f);

			zoom.Update(io.DeltaTime);
			mini_map_fade.Update(io.DeltaTime);

			camera.zoom = camera.ClampZoom(zoom.GetAlpha(), MinGridSize(), MaxGridSize());
			camera.position = camera.ClampLocation(camera.position);
		}

		FCamera& Camera() {
			return camera;
		}

		const FCamera& Camera() const {
			return camera;
		}

		FMinimapState& MinimapState() {
			return minimap_state;
		}

		FTexture& GetTexture() {
			return texture;
		}

		float MinGridSize() const {
			return static_cast<float>(tile_size) * min_tiles_space_size;
		}

		float MaxGridSize() const {
			return static_cast<float>(tile_size) * tiles_space_size;
		}

		ImU32 MiniMapColor() const {
			return mini_map_fade.Color();
		}

		float MiniMapColorAlpha() const {
			return mini_map_fade.GetAlpha();
		}

	private:
		void Zoom(float in_zoom_step)
		{
			const float zoom_factor = in_zoom_step > 0 ? 1.1f : 0.9f;
			const auto target_zoom = camera.ClampZoom(zoom.GetAlpha() * zoom_factor, MinGridSize(), MaxGridSize());

			zoom.SetTarget(target_zoom);
		}

		const std::uint16_t tile_size;
		const float min_tiles_space_size;
		const float tiles_space_size;

		FColorInterpolation mini_map_fade{ {1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 10.5f, EInterpolationType::Sinusoidal };
		FInterpolation zoom;
		FCamera camera;
		FMinimapState minimap_state;
		FTexture texture;
	};

	struct FCursorScreenGuard {
		FCursorScreenGuard(const ImVec2& in_pos) {
			ImGui::SetCursorScreenPos(in_pos);
		}

		~FCursorScreenGuard() {
			ImGui::SetCursorScreenPos(cached_cursor);
		}
	private:
		ImVec2 cached_cursor = ImGui::GetCursorScreenPos();
	};

	void draw_minimap(ImDrawList* draw_list, FCamera& camera, FMinimapState& minimap_state, const ImVec2& minimap_pos, const ImVec2& minimap_size, float alpha) {
		if (std::abs(alpha) > 0.1f) {
			const FCursorScreenGuard cursor_guard(minimap_pos);

			ImGui::InvisibleButton("mini_map", minimap_size);

			draw_list->AddRectFilled(minimap_pos, minimap_pos + minimap_size, IM_COL32(50, 50, 50, 255 * alpha));
			{
				const auto visible_world = camera.VisibleWorldBounds();

				const auto camera_min_screen = camera.WorldToMinimap(visible_world.min, minimap_pos, minimap_size);
				const auto camera_max_screen = camera.WorldToMinimap(visible_world.max, minimap_pos, minimap_size);

				draw_list->AddRect(camera_min_screen, camera_max_screen, IM_COL32(0, 255, 0, 255 * alpha), 0.0f, 0, 2.0f);
			}
			draw_list->AddRect(minimap_pos, minimap_pos + minimap_size, IM_COL32(255, 255, 255, 255 * alpha));

			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				ImVec2 mouse_pos = ImGui::GetMousePos();
				camera.position = camera.MinimapToWorld(minimap_pos, minimap_size, mouse_pos);
				minimap_state.is_dragging = true;
				minimap_state.last_mouse_pos = mouse_pos;
			}

			if (minimap_state.is_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
				ImVec2 current_mouse_pos = ImGui::GetMousePos();
				ImVec2 delta = current_mouse_pos - minimap_state.last_mouse_pos;

				ImVec2 world_delta = delta / ImVec2(minimap_size.x / camera.world_bounds.Width(), minimap_size.y / camera.world_bounds.Height());
				camera.position += world_delta;
				minimap_state.last_mouse_pos = current_mouse_pos;
			}

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				minimap_state.is_dragging = false;
			}
		}
	}

	ImVec2 fit_texture(const FTexture& in_texture, const FCamera& in_camera) {
		const auto world_size = in_camera.world_bounds.Size();
		const auto scale_vec = world_size / ImVec2(in_texture.get_width(), in_texture.get_height());

		const float scale = std::min(scale_vec.x, scale_vec.y);

		return {in_texture.get_width() * scale, in_texture.get_height() * scale};
	}

	ImVec2 ImRotate(const ImVec2& v, float cos_a, float sin_a) {
		return {v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a};
	}
	void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle) {
		ImDrawList* draw_list = ImGui::GetWindowDrawList();

		const auto cos_a = cosf(angle);
		const auto sin_a = sinf(angle);
		const ImVec2 pos[4] = {
			center + ImRotate(ImVec2(-size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
			center + ImRotate(ImVec2(+size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
			center + ImRotate(ImVec2(+size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a),
			center + ImRotate(ImVec2(-size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a)
		};
		const ImVec2 uvs[4] = {
			ImVec2(0.0f, 0.0f),
			ImVec2(1.0f, 0.0f),
			ImVec2(1.0f, 1.0f),
			ImVec2(0.0f, 1.0f)
		};

		draw_list->AddImageQuad(tex_id, pos[0], pos[1], pos[2], pos[3], uvs[0], uvs[1], uvs[2], uvs[3], IM_COL32_WHITE);
	}

	void sprite_editor_widget(FSpriteEditorViewModel& sprite_editor_view_model)
	{
		auto&& width = ImGui::CalcItemWidth();
		auto&& position = ImGui::GetCursorScreenPos();

		const ImVec2 viewport_size = { width, width };
		const ImVec2 viewport_top_left = position;
		const ImVec2 viewport_bottom_right = { position.x + width, position.y + width };

		ImGui::SetNextItemAllowOverlap();
		ImGui::InvisibleButton("sprite_editor_widget", viewport_size);

		sprite_editor_view_model.Update(viewport_top_left, viewport_bottom_right);

		if (auto* draw_list = ImGui::GetWindowDrawList())
		{
			ImGui::PushClipRect(viewport_top_left, viewport_bottom_right, true);

			const auto zoom = sprite_editor_view_model.Camera().zoom;

			draw_list->AddText(position, IM_COL32(128, 128, 128, 255), std::to_string(zoom).c_str());

			draw_grid_with_camera(draw_list, sprite_editor_view_model.Camera(), 8.0f * 2, IM_COL32(128, 128, 128, 255), position);

			if (auto&& texture = sprite_editor_view_model.GetTexture())
			{
				auto&& texture_size = fit_texture(texture, sprite_editor_view_model.Camera());

				auto&& min = sprite_editor_view_model.Camera().WorldToScreen(-texture_size / 2.0f);
				auto&& max = sprite_editor_view_model.Camera().WorldToScreen(texture_size / 2.0f);

				auto&& center = sprite_editor_view_model.Camera().WorldToScreen({ 0.0f, 0.0f });
				auto&& size = max - min;

				static float rotation_angle = 0.0f;
				rotation_angle += 0.35f * ImGui::GetIO().DeltaTime;

				ImageRotated(texture.get_texture(), center, size, rotation_angle);
			}

			if (auto&& mini_map_size = viewport_size * 0.1f; mini_map_size.x > 0.0f) {
				auto&& mini_map_pos = viewport_bottom_right - mini_map_size - mini_map_size * 0.1f;

				draw_minimap(draw_list, sprite_editor_view_model.Camera(), sprite_editor_view_model.MinimapState(), mini_map_pos, mini_map_size, sprite_editor_view_model.MiniMapColorAlpha());
			}

			draw_list->AddQuad(position, { position.x + width, position.y }, { position.x + width, position.y + width }, { position.x, position.y + width }, IM_COL32(128, 128, 128, 255), 3.0f);

			ImGui::PopClipRect();
		}
	}

	void draw_sprite_editor_window(FSpriteEditorViewModel& sprite_editor_view_model)
	{
		if (auto sprite_editor = ym::sprite_editor::create_sprite_editor())
		{
			sprite_editor->create_sprite({0, 0});
			sprite_editor->create_sprite({8, 8});
			sprite_editor->create_sprite({16, 16});

			// if (const auto sprites = sprite_editor->sprites())
			{
				for (auto sprite : sprite_editor->sprites())
				{
					std::cout << sprite->position << "\n";
				}
			}
		}

		if (ImGui::Begin("Sprite Editor"))
		{
			ImGui::Button("Header");

			auto&& Space = ImGui::GetContentRegionAvail();

			ImGui::PushItemWidth(Space.x * 0.5f);
			sprite_editor_widget(sprite_editor_view_model);
			ImGui::PopItemWidth();

			ImGui::Button("Footer");

			ImGui::End();
		}
	}
}

struct FSpriteEditorApplication
{
	const uint32_t SCREEN_WIDTH = 1920;
	const uint32_t SCREEN_HEIGHT = 1080;

	void setup_imgui_context(SDL_Window* window, SDL_Renderer* renderer)
	{
		// setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		(void)io;

		if (std::filesystem::exists("Content/Font/font.ttf"))
		{
			if (auto* font = io.Fonts->AddFontFromFileTTF("Content/Font/font.ttf", 16.0f))
			{
				font->Scale = 1.0f;
			}
		}

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable keyboard controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		// io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

		io.MouseDrawCursor = true;

		// theme
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
		ImGui_ImplSDLRenderer2_Init(renderer);

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	void main_loop() const
	{
		bool should_quit = false;

		while (!should_quit)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL2_ProcessEvent(&event);
				if (event.type == SDL_QUIT)
				{
					should_quit = true;
					break;
				}
			}

			ImGui::NewFrame();

			ym::ui::draw_sprite_editor_window(sprite_editor_view_mode);

			ImGui::Render();
			ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);

			SDL_RenderPresent(renderer_);

			SDL_RenderClear(renderer_);
		}
	}

	int entry()
	{
		if (SDL_Init(SDL_INIT_EVERYTHING) == 0)
		{
			constexpr SDL_WindowFlags window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
			if (window_ = SDL_CreateWindow("Resources Explorer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, window_flags); window_ != nullptr)
			{
				if (renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_PRESENTVSYNC); renderer_ != nullptr)
				{
					setup_imgui_context(window_, renderer_);
					{
						int width, height, channels;
						if (auto* data = stbi_load("data/hedgehog.png", &width, &height, &channels, 0))
						{
							sprite_editor_view_mode.GetTexture().Load(data, channels, width, height, renderer_);
							stbi_image_free(data);
						}
					}

					main_loop();
				}
			}
		}

		return 0;
	}

	~FSpriteEditorApplication()
	{
		ImGui_ImplSDLRenderer2_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		SDL_DestroyRenderer(renderer_);
		SDL_DestroyWindow(window_);

		SDL_Quit();
	}

private:
	SDL_Window* window_ = nullptr;
	SDL_Renderer* renderer_ = nullptr;

	mutable ym::ui::FSpriteEditorViewModel sprite_editor_view_mode;
};

int main()
{
	FSpriteEditorApplication application;
	return application.entry();
}
