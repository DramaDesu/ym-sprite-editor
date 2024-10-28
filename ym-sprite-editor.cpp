#include "lib/include/ym-sprite-editor.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#define SDL_MAIN_HANDLED
#include <filesystem>

#include "SDL.h"

#define STB_IMAGE_IMPLEMENTATION 
#include <iostream>

#include "stb_image.h"

using namespace std;

namespace ym::ui {

	ImVec2 operator+(const ImVec2& in_vector_a, const ImVec2& in_vector_b)
	{
		return { in_vector_a.x + in_vector_b.x, in_vector_a.y + in_vector_b.y };
	}

	ImVec2 ImRotate(const ImVec2& v, float cos_a, float sin_a) {
		return { v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a };
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


	class FTexture
	{
	public:
		void Load(std::uint8_t* in_data, size_t in_depth, int in_width, int in_height, SDL_Renderer* in_renderer)
		{
			constexpr auto red_mask = 0x000000ff;
			constexpr auto green_mask = 0x0000ff00;
			constexpr auto blue_mask = 0x00ff0000;
			constexpr auto alpha_mask = 0xff000000;

			if (SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(in_data, in_width, in_height, in_depth * 8, in_depth * in_width, red_mask, green_mask, blue_mask, alpha_mask)) 
			{
				if (auto texture = SDL_CreateTextureFromSurface(in_renderer, surface))
				{
					data_ = std::make_shared<shared_data>(texture, in_width, in_height);
				}
				SDL_FreeSurface(surface);
			}
		}

		operator bool() const {
			return data_ && data_->texture_;
		}

		SDL_Texture* get_texture() const { return data_ ? data_->texture_ : nullptr; }
		float get_width() const { return data_ ? data_->width_ : 0; }
		float get_height() const { return data_ ? data_->height_ : 0; }

	private:
		struct shared_data
		{
			shared_data() = default;
			shared_data(SDL_Texture* in_texture, float in_width, float in_height) : texture_(in_texture), width_(in_width), height_(in_height) {}

			shared_data(const shared_data& other) = default;
			shared_data& operator=(const shared_data& other) = default;

			shared_data(shared_data&& other) noexcept = default;
			shared_data& operator=(shared_data&& other) = default;

			~shared_data()
			{
				SDL_DestroyTexture(texture_);
			}

			SDL_Texture* texture_ = nullptr;
			float width_ = 0;
			float height_ = 0;
		};

		std::shared_ptr<shared_data> data_ = nullptr;
	};

	class TextureSprite : public sprite_editor::BaseSprite
	{
	public:
		size_t type() const override { return sprite_editor::types::type_id<TextureSprite>(); }

		glm::vec2 get_size() const override
		{
			return {texture.get_width() * scale, texture.get_height() * scale };
		}

		FTexture texture;
		float rotation = 0.0f;
		float rotation_speed = 0.0f;
		float scale = 1.0f;
	};

	ym::sprite_editor::vec2 fit_texture(const ym::ui::FTexture& in_texture, const ym::sprite_editor::vec2& in_world_bounds) {
		const auto scale_vec = in_world_bounds / ym::sprite_editor::vec2{ in_texture.get_width(), in_texture.get_height() };
		const float scale = std::min(scale_vec.x, scale_vec.y);
		return { in_texture.get_width() * scale, in_texture.get_height() * scale };
	}

	void draw_sprite_editor_window(const std::shared_ptr<sprite_editor::ISpriteEditor>& sprite_editor)
	{
		if (ImGui::Begin("Sprite Editor"))
		{
			auto&& Space = ImGui::GetContentRegionAvail();

			ImGui::PushItemWidth(Space.x * 0.5f);
			draw_sprite_editor(sprite_editor);
			ImGui::PopItemWidth();

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

			ym::ui::draw_sprite_editor_window(sprite_editor);

			ImGui::Render();
			ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);

			SDL_RenderPresent(renderer_);

			SDL_RenderClear(renderer_);
		}
	}

	void register_sprite_renderings(const shared_ptr<ym::sprite_editor::ISpriteEditor>& editor) const
	{
		editor->register_sprite_renderer<ym::ui::TextureSprite>([editor](const auto& in_sprite)
		{
			auto&& texture_sprite = std::static_pointer_cast<ym::ui::TextureSprite>(in_sprite);
			if (auto&& texture = texture_sprite->texture.get_texture())
			{
				auto&& screen_location = editor->world_to_screen(in_sprite->position);
				auto&& screen_size = editor->world_size_to_screen_size(in_sprite->get_size());

				ym::ui::ImageRotated(texture, {screen_location.x, screen_location.y}, { screen_size.y, screen_size.y}, texture_sprite->rotation);

				texture_sprite->rotation += texture_sprite->rotation_speed * ImGui::GetIO().DeltaTime;
			}
		});

		editor->register_sprite_details_renderer<ym::ui::TextureSprite>([editor](auto& in_sprite)
		{
			if (ImGui::BeginChild("sprite_details"))
			{
				ImGui::TextDisabled("texture sprite");

				auto&& world_bounds= editor->world_bounds();

				auto&& texture_sprite = std::static_pointer_cast<ym::ui::TextureSprite>(in_sprite);

				float position[2] = { in_sprite->position.x, in_sprite->position.y };
				if (ImGui::SliderFloat2("location", position, -world_bounds.x, world_bounds.x))
				{
					in_sprite->position.x = position[0];
					in_sprite->position.y = position[1];
				}

				ImGui::LabelText("size", "%fx%f", in_sprite->get_size().x, in_sprite->get_size().y);
				ImGui::LabelText("rotation", "%f", texture_sprite->rotation);
				ImGui::LabelText("rotation_speed", "%f", texture_sprite->rotation_speed);
				ImGui::LabelText("scale", "%f", texture_sprite->scale);

				ImGui::EndChild();
			}
		});
	}

	void add_texture_sprite(const shared_ptr<ym::sprite_editor::ISpriteEditor>& editor) const
	{
		int width, height, channels;
		if (auto* data = stbi_load("data/hedgehog.png", &width, &height, &channels, 0))
		{
			srand(static_cast<unsigned>(time(nullptr)));
			auto normalized_random = [] { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); };

			editor->register_sprite<ym::ui::TextureSprite>();

			ym::ui::FTexture texture;
			texture.Load(data, channels, width, height, renderer_);

			for (int i = 0; i < 10; ++i)
			{
				if (auto&& sprite = editor->create_sprite<ym::ui::TextureSprite>())
				{
					sprite->texture = texture;
					{
						auto&& sprite_size = sprite->get_size();

						auto&& location = ym::sprite_editor::vec2{ sprite_size.x, 0 } * i * 1.0f;
						sprite->position.x = location.x;
						sprite->position.y = location.y;
					}

					// sprite->scale = 0.75f - i * 0.05f;
					sprite->rotation = normalized_random() * 90.0f;
					sprite->rotation_speed = 0.35f + normalized_random() * 0.55f;
				}
			}
			stbi_image_free(data);
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

					if (auto&& created_sprite_editor = ym::sprite_editor::create_sprite_editor())
					{
						register_sprite_renderings(created_sprite_editor);
						add_texture_sprite(created_sprite_editor);

						sprite_editor = created_sprite_editor;
						sprite_editor->set_grid_cell_size(128);
						sprite_editor->setup_snap({1, 128});
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

	std::shared_ptr<ym::sprite_editor::ISpriteEditor> sprite_editor;
};

int main()
{
	FSpriteEditorApplication application;
	return application.entry();
}