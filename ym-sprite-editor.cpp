// ym-sprite-editor.cpp : Defines the entry point for the application.
//

#include "ym-sprite-editor.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#define SDL_MAIN_HANDLED
#include <filesystem>

#include "SDL.h"

using namespace std;

namespace ym::ui
{
	void draw_sprite_editor_window()
	{
		if (ImGui::Begin("Sprite Editor"))
		{
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

			ym::ui::draw_sprite_editor_window();

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
};

int main()
{
	FSpriteEditorApplication application;
	return application.entry();
}
