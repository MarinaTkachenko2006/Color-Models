#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Структура изображения
struct ImageRGB
{
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;        // size = width * height * 3

    bool empty() const { return data.empty(); }

    uint8_t* at(int x, int y)
    {
        return &data[(static_cast<size_t>(y) * width + x) * 3];
    }
    const uint8_t* at(int x, int y) const
    {
        return &data[(static_cast<size_t>(y) * width + x) * 3];
    }
};

bool loadImageRGB(const std::string& path, ImageRGB& out)
{
    int w = 0, h = 0, srcChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &srcChannels, 3);
    if (!pixels)
    {
        std::fprintf(stderr, "stbi_load failed for '%s': %s\n",
            path.c_str(), stbi_failure_reason());
        return false;
    }

    out.width = w;
    out.height = h;
    out.channels = srcChannels;
    out.data.assign(pixels, pixels + static_cast<size_t>(w) * h * 3);

    stbi_image_free(pixels);
    return true;
}

// Диалог выбора файла.
struct FileDialogState
{
    bool pending = false;
    bool ready = false;
    bool ok = false;
    std::string path;

    void reset() { ready = false; ok = false; path.clear(); }
};

static void SDLCALL onFileDialogResult(void* userdata,
    const char* const* filelist,
    int /*filter*/)
{
    auto* st = static_cast<FileDialogState*>(userdata);
    st->pending = false;
    st->ready = true;

    if (!filelist || !filelist[0]) { st->ok = false; return; }
    st->ok = true;
    st->path = filelist[0];
}

// ---------------------------------------------------------------------------
// Пересоздание SDL-текстуры из матрицы.
// ---------------------------------------------------------------------------
void updateTexture(SDL_Renderer* renderer, SDL_Texture*& tex,
    const ImageRGB& image)
{
    if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
    if (image.empty()) return;

    tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STATIC,
        image.width, image.height);
    if (!tex) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return;
    }
    SDL_UpdateTexture(tex, nullptr, image.data.data(), image.width * 3);
}

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("ImGui Window", 1000, 700, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    ImageRGB image;
    SDL_Texture* tex = nullptr;
    FileDialogState dlg;

    const SDL_DialogFileFilter filters[] = {
        { "Images", "png;jpg;jpeg;bmp;tga;gif;psd;hdr;pic;pnm" },
        { "All files", "*" }
    };

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        if (dlg.ready) {
            if (dlg.ok && loadImageRGB(dlg.path, image)) {
                updateTexture(renderer, tex, image);
            }
            dlg.reset();
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        const float panelW = 260.0f;
        const float panelH = 120.0f;
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelW - 10.0f, 10.0f),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

        ImGui::Begin("Panel", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse);

        if (ImGui::Button("load image")) {
            if (!dlg.pending) {
                dlg.pending = true;
                SDL_ShowOpenFileDialog(
                    onFileDialogResult, &dlg, window,
                    filters,
                    static_cast<int>(sizeof(filters) / sizeof(filters[0])),
                    nullptr, false);
            }
        }

        if (ImGui::Button("Task 1")) {
            // TODO: задание 1
        }

        if (ImGui::Button("Task 2")) {
            // TODO: задание 2
        }

        if (ImGui::Button("Task 3")) {
            // TODO: задание 3
        }

        if (dlg.pending)
            ImGui::TextUnformatted("dialog is open...");

        if (!image.empty())
            ImGui::Text("size: %d x %d", image.width, image.height);

        ImGui::End();

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        if (tex) {
            SDL_FRect dst{ 0.f, 0.f, (float)image.width, (float)image.height };

            // Библиотечное отображение изображения - поменять на отрисовку вручную???
            SDL_RenderTexture(renderer, tex, nullptr, &dst);
        }

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    if (tex) SDL_DestroyTexture(tex);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}