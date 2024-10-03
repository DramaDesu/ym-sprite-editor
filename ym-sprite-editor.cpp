#include "lib/include/ym-sprite-editor.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#define SDL_MAIN_HANDLED
#include <filesystem>

#include "SDL.h"

#define STB_IMAGE_IMPLEMENTATION 
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

	struct TextureSprite : sprite_editor::BaseSprite
	{
		size_t type() const override { return sprite_editor::type_id<TextureSprite>(); }

		sprite_editor::size_t get_size() const override
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
			ImGui::Button("Header");

			auto&& Space = ImGui::GetContentRegionAvail();

			ImGui::PushItemWidth(Space.x * 0.5f);
			draw_sprite_editor(sprite_editor);
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
				auto&& screen_location = editor->world_to_screen({ in_sprite->position.x, in_sprite->position.y });
				auto&& screen_size = 
					editor->world_to_screen(ym::sprite_editor::vec2{ in_sprite->position.x, in_sprite->position.y } +
						ym::sprite_editor::vec2{in_sprite->get_size().x, in_sprite->get_size().y}) - screen_location;

				ym::ui::ImageRotated(texture,
					ym::sprite_editor::ToImVec2(screen_location),
						ym::sprite_editor::ToImVec2(screen_size), texture_sprite->rotation);

				texture_sprite->rotation += texture_sprite->rotation_speed * ImGui::GetIO().DeltaTime;
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

			for (int i = 0; i < 10; ++i)
			{
				if (auto&& sprite = std::make_shared<ym::ui::TextureSprite>())
				{
					sprite->texture.Load(data, channels, width, height, renderer_);
					sprite->scale = 1.0f - i * 0.05f;

					auto&& location = ym::sprite_editor::vec2{ sprite->get_size().x, sprite->get_size().y } * normalized_random();
					{
						sprite->position.x = location.x;
						sprite->position.y = location.y;
					}

					sprite->rotation = normalized_random() * 90.0f;
					sprite->rotation_speed = 0.35f + normalized_random() * 0.55f;

					editor->add_sprite(sprite);
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

					if (sprite_editor = ym::sprite_editor::create_sprite_editor(); sprite_editor)
					{
						register_sprite_renderings(sprite_editor);
						add_texture_sprite(sprite_editor);
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