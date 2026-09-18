#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
#include <array>
#include <iostream>

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

struct ImageGray
{
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;        // size = width * height

    bool empty() const { return data.empty(); }

    uint8_t* at(int x, int y)
    {
        return &data[(static_cast<size_t>(y) * width + x)];
    }
    const uint8_t* at(int x, int y) const
    {
        return &data[(static_cast<size_t>(y) * width + x)];
    }
};

// Отображение изображения вручную по пикселям
void drawImageByPixels(SDL_Renderer* renderer, const ImageRGB& img,
    float originX = 0.f, float originY = 0.f)
{
    if (img.empty()) return;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* p = img.at(x, y);
            SDL_SetRenderDrawColor(renderer, p[0], p[1], p[2], 255);
            SDL_RenderPoint(renderer, originX + x, originY + y);
        }
    }
}
void drawImageByPixels(SDL_Renderer* renderer, const ImageGray& img,
    float originX = 0.f, float originY = 0.f)
{
    if (img.empty()) return;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t v = *img.at(x, y);
            SDL_SetRenderDrawColor(renderer, v, v, v, 255);
            SDL_RenderPoint(renderer, originX + x, originY + y);
        }
    }
}

void drawImageByTexture(SDL_Renderer* renderer, SDL_Texture* texture, float x, float y) {
    if (!renderer || !texture) return;

    float weight, height;
    SDL_GetTextureSize(texture, &weight, &height);
    SDL_FRect dst{ x, y, weight, height };
    SDL_RenderTexture(renderer, texture, nullptr, &dst);
}

ImageGray toGray(const ImageRGB& img, bool formula) {
    ImageGray imgGr;
    imgGr.channels = 1;
    imgGr.width = img.width;
    imgGr.height = img.height;
    imgGr.data = std::vector<uint8_t>(img.width * img.height);

    if (formula) // NTSC RGB
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                const uint8_t* p = img.at(x, y);
                imgGr.data[static_cast<size_t>(y) * imgGr.width + x] = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
            }
        }
    else  // sRGB
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                const uint8_t* p = img.at(x, y);
                imgGr.data[static_cast<size_t>(y) * imgGr.width + x] = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
            }
        }
    return imgGr;
}

ImageGray diffGray(const ImageGray& img1, const ImageGray& img2) {
    ImageGray imgDiff;
    imgDiff.channels = 1;
    imgDiff.width = img1.width;
    imgDiff.height = img1.height;
    imgDiff.data = std::vector<uint8_t>(img1.width * img1.height);

    int minDiff = 255;
    int maxDiff = 0;

    // Поиск отличий
    for (int y = 0; y < img1.height; ++y) {
        for (int x = 0; x < img1.width; ++x) {
            int diff = std::abs(static_cast<int>(*img1.at(x, y)) -
                static_cast<int>(*img2.at(x, y)));
            imgDiff.data[static_cast<size_t>(y) * imgDiff.width + x] =
                static_cast<uint8_t>(diff);
            if (diff < minDiff) minDiff = diff;
            if (diff > maxDiff) maxDiff = diff;
        }
    }

    // Нормализация
    if (maxDiff > minDiff) {
        const double scale = 255.0 / (maxDiff - minDiff);
        for (auto& v : imgDiff.data) {
            v = static_cast<uint8_t>((v - minDiff) * scale + 0.5);
        }
    }
    else {
        std::fill(imgDiff.data.begin(), imgDiff.data.end(), 0);
    }
    
    return imgDiff;
}

std::array<int, 256> intensityHistogram(const ImageGray& img)
{
    std::array<int, 256> h{};
    for (uint8_t v : img.data)
        ++h[v];
    return h;
}
// Отображение гистограммы вручную
void drawHist(SDL_Renderer* renderer,
    const std::array<int, 256>& hgt,
    float x0, float y0, float w, float h,
    Uint8 r = 255, Uint8 g = 255, Uint8 b = 255)
{
    // Ищем максимум, чтобы нормировать
    int maxH = 0;
    for (int v : hgt) if (v > maxH) maxH = v;
    if (maxH == 0 || w <= 0.f || h <= 0.f) return;

    const float baseY = y0 + h;   // основание столбиков
    const float scale = h / static_cast<float>(maxH);

    // Рисуем столбики
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int v = 0; v < 256; ++v) {
        float xv = x0 + (static_cast<float>(v) + 0.5f) * (w / 256.f);
        float barH = hgt[v] * scale;
        if (barH < 1.f && hgt[v] > 0) barH = 1.f;

        SDL_RenderLine(renderer, xv, baseY, xv, baseY - barH);
    }
}

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

SDL_Texture* makeTextureRGB(SDL_Renderer* r, const ImageRGB& img)
{
    if (img.empty()) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, img.data.data(), img.width * 3);
    return t;
}

SDL_Texture* makeTextureGray(SDL_Renderer* r, const ImageGray& img)
{
    if (img.empty()) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (!t) return nullptr;

    std::vector<uint8_t> rgb(static_cast<size_t>(img.width) * img.height * 3);
    for (size_t i = 0; i < img.data.size(); ++i) {
        uint8_t v = img.data[i];
        rgb[i * 3 + 0] = v;
        rgb[i * 3 + 1] = v;
        rgb[i * 3 + 2] = v;
    }
    SDL_UpdateTexture(t, nullptr, rgb.data(), img.width * 3);
    return t;
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

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init is failed" << std::endl;
        return -1;
    }
    SDL_Window* window = SDL_CreateWindow("ImGui Window", 1000, 700, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow is failed" << std::endl;
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
    {
        std::cerr << "SDL_CreateRenderer is failed" << std::endl;
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    ImageRGB image;
    SDL_Texture* texImage = nullptr;
    SDL_Texture* texImg1 = nullptr;
    SDL_Texture* texImg2 = nullptr;
    SDL_Texture* texDiff = nullptr;

    FileDialogState dlg;
    bool showWindow2 = false;

    std::vector<SDL_FPoint> pendingDrawPos;     // Координаты, куда рисовать картинки
    std::vector<SDL_FPoint> pendingHistPos;     // Координаты, куда рисовать гистограммы

    const SDL_DialogFileFilter filters[] = {
        { "Images", "png;jpg;jpeg;bmp;tga;gif;psd;hdr;pic;pnm" },
        { "All files", "*" }
    };

    bool running (true);
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
                // Загрузка изображения
                if (texImage) SDL_DestroyTexture(texImage);
                if (texImg1)  SDL_DestroyTexture(texImg1);
                if (texImg2)  SDL_DestroyTexture(texImg2);
                if (texDiff)  SDL_DestroyTexture(texDiff);

                auto img1 = toGray(image, true);
                auto img2 = toGray(image, false);
                auto imgDiff = diffGray(img1, img2);

                texImage = makeTextureRGB(renderer, image);
                texImg1 = makeTextureGray(renderer, toGray(image, true));
                texImg2 = makeTextureGray(renderer, toGray(image, false));
                texDiff = makeTextureGray(renderer, diffGray(img1, img2));

            }
            dlg.reset();
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        pendingDrawPos.clear();
        pendingHistPos.clear();

        // Панель и кнопки
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
            showWindow2 = true;
        }

        if (ImGui::Button("Task 2")) {
            // TODO: задание 2
        }

        if (ImGui::Button("Task 3")) {
            // TODO: задание 3
        }

        ImGui::End();

        if (showWindow2) {
            ImGui::SetNextWindowPos(ImVec2(200.f, 200.f), ImGuiCond_FirstUseEver);
            ImGui::Begin("Window 2", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_HorizontalScrollbar |
                ImGuiWindowFlags_AlwaysAutoResize);

            if (ImGui::Button("close")) {
                showWindow2 = false;
            }
            ImGui::NewLine();

            if (!image.empty()) {
                const ImVec2 imgSz((float)image.width, (float)image.height);
                const ImVec2 histSz((float)image.width, 100.f);   // ширина = картинке
                const float  gap = 6.f;

                for (int k = 0; k < 3; ++k) {
                    // Картинка слева.
                    ImVec2 pImg = ImGui::GetCursorScreenPos();
                    pendingDrawPos.push_back(SDL_FPoint{ pImg.x, pImg.y });
                    ImGui::Dummy(imgSz);

                    // Гистограмма справа от картинки.
                    ImGui::SameLine(0.f, gap);
                    ImVec2 pHist = ImGui::GetCursorScreenPos();
                    pendingHistPos.push_back(SDL_FPoint{ pHist.x, pHist.y });
                    ImGui::Dummy(histSz);

                }
            }

            ImGui::End();
        }

        ImGui::Render();

        // Отображение изображений
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        //drawImageByPixels(renderer, image); // Изображение, открытое в главном окне
        drawImageByTexture(renderer, texImage, 0, 0);

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        // Изображения в окне, связанном с первым заданием: приведение к оттенкам серого
        if (pendingDrawPos.size() > 0) {
            drawImageByTexture(renderer, texImg1, pendingDrawPos[0].x, pendingDrawPos[0].y);
            drawImageByTexture(renderer, texImg2, pendingDrawPos[1].x, pendingDrawPos[1].y);
            drawImageByTexture(renderer, texDiff, pendingDrawPos[2].x, pendingDrawPos[2].y);


            auto img1 = toGray(image, true);    // NTSC RGB
            auto img2 = toGray(image, false);   // sRGB
            auto imgDiff = diffGray(img1, img2);// Разница

            // Вывод результатов преобразований
            //drawImageByPixels(renderer, img1, pendingDrawPos[0].x, pendingDrawPos[0].y);  
            //drawImageByPixels(renderer, img2, pendingDrawPos[1].x, pendingDrawPos[1].y); 
            //drawImageByPixels(renderer,imgDiff, pendingDrawPos[2].x, pendingDrawPos[2].y);

            // Гистограммы интенсивности - каждая справа от изображения

            drawHist(renderer, intensityHistogram(img1),
                pendingHistPos[0].x, pendingHistPos[0].y,
                (float)image.width, (float)image.height);

            drawHist(renderer, intensityHistogram(img2),
                pendingHistPos[1].x, pendingHistPos[1].y,
                (float)image.width, (float)image.height);

            drawHist(renderer, intensityHistogram(imgDiff),
                pendingHistPos[2].x, pendingHistPos[2].y,
                (float)image.width, (float)image.height);
        }

        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}