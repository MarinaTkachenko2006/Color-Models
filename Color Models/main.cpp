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
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Структура изображения
struct ImageRGB
{
    int width = 0; // Ширина изображения
    int height = 0; // Высота изображения
    int channels = 0; // Количество каналов
    std::vector<uint8_t> data; // Вектор битов размера width * height * 3

    bool empty() const noexcept { return data.empty(); }

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
    int width = 0; // Ширина изображения
    int height = 0; // Высота изображения
    int channels = 0; // Количество каналов
    std::vector<uint8_t> data; // Вектор битов размера width * height

    bool empty() const noexcept { return data.empty(); }

    uint8_t* at(int x, int y)
    {
        return &data[(static_cast<size_t>(y) * width + x)];
    }
    const uint8_t* at(int x, int y) const
    {
        return &data[(static_cast<size_t>(y) * width + x)];
    }
};

// Отображение RGB-изображения вручную по пикселям (неэффективное)
void drawImageByPixels(SDL_Renderer* renderer, const ImageRGB& img,
    float originX = 0, float originY = 0) // Смещение на экране
{
    if (img.empty()) return;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* p = img.at(x, y); // Указатель на текущий пиксель
            SDL_SetRenderDrawColor(renderer, p[0], p[1], p[2], 255); 
            SDL_RenderPoint(renderer, originX + x, originY + y);
        }
    }
}

// Отображение Gray-изображения вручную по пикселям (неэффективное)
void drawImageByPixels(SDL_Renderer* renderer, const ImageGray& img,
    float originX = 0, float originY = 0) // Смещение на экране
{
    if (img.empty()) return;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t v = *img.at(x, y); // Указатель на текущий пиксель
            SDL_SetRenderDrawColor(renderer, v, v, v, 255);
            SDL_RenderPoint(renderer, originX + x, originY + y);
        }
    }
}

// Преобразование RGB-изображения в Gray-изображение
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

// Разница между 2 GRAY-изображениями
ImageGray diffGray(const ImageGray& img1, const ImageGray& img2) {
    ImageGray imgDiff; // Результат
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

    // Нормализация (чтобы лучше было восприятие)
    if (maxDiff > minDiff) {
        const double scale = 255.0 / (maxDiff - minDiff);
        for (auto& v : imgDiff.data)
            v = static_cast<uint8_t>((v - minDiff) * scale + 0.5);
    }
    else std::fill(imgDiff.data.begin(), imgDiff.data.end(), 0);
    
    return imgDiff;
}

// Гистограмма яркости Gray-изображения
std::array<int, 256> intensityHistogram(const ImageGray& img)
{
    std::array<int, 256> h{};
    for (uint8_t v : img.data) ++h[v];

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
    if (maxH == 0 || w <= 0 || h <= 0) return;

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


void drawHistImGui(ImDrawList* dl, ImVec2 origin, ImVec2 size,
    const std::array<int, 256>& hgt, ImU32 color)
{
    int maxH = 0;
    for (int v : hgt) if (v > maxH) maxH = v;
    if (maxH == 0 || size.x <= 0 || size.y <= 0) return;

    float baseY = origin.y + size.y;
    float scale = size.y / static_cast<float>(maxH);
    float dx = size.x / 256.0f;

    for (int v = 0; v < 256; ++v) {
        float xv = origin.x + (v + 0.5f) * dx;
        float barH = hgt[v] * scale;
        if (barH < 1.f && hgt[v] > 0) barH = 1.f;
        dl->AddLine(ImVec2(xv, baseY), ImVec2(xv, baseY - barH), color);
    }
}

// Загрузка из файловой системы RGB-изображения
bool loadImageRGB(const std::string& path, ImageRGB& out)
{
    int w = 0, h = 0, srcChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &srcChannels, 3);

    if (!pixels)
    {
        std::cerr << "stbi_load failed for '" << path << "': " << stbi_failure_reason() << std::endl;
        return false;
    }

    out.width = w;
    out.height = h;
    out.channels = srcChannels;
    out.data.assign(pixels, pixels + static_cast<size_t>(w) * h * 3); // Копирование массива указателей pixels в массив out.data

    stbi_image_free(pixels); // Освобождение буфера, который stbi_load выделил внутри себя через malloc

    return true;
}

// Преобразование RGB-изображения в SDL_Texture
SDL_Texture* makeTextureRGB(SDL_Renderer* r, const ImageRGB& img)
{
    if (img.empty()) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, img.data.data(), img.width * 3);
    return t;
}

// Преобразование Gray-изображения в SDL_Texture
SDL_Texture* makeTextureGray(SDL_Renderer* r, const ImageGray& img)
{
    if (img.empty()) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
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


// Диалог выбора файла
struct FileDialogState
{
    bool pending = false; // Флаг открытости окна
    bool ready = false; // Флаг того, можно обрабатывать результат 
    bool ok = false; // Флаг того, что пользователь открыл файл
    std::string path; // Путь к изображению

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

int main(int argc, char* argv[])
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
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    ImGuiIO& io = ImGui::GetIO(); // Глобальный ввод-вывод
    SDL_Event eventer;

    ImageRGB image;
    SDL_Texture* texImage = nullptr;
    SDL_Texture* texImg1 = nullptr;
    SDL_Texture* texImg2 = nullptr;
    SDL_Texture* texDiff = nullptr;

    FileDialogState dlg; // Диалоговое окно
    bool showWindow2 = false; // Флаг открытости 2го окна
    enum class ActiveTask { None, Task1, Task2, Task3 };
    ActiveTask activeTask = ActiveTask::None;
    const SDL_DialogFileFilter filters[] = { { "Images", "png;jpg;jpeg;bmp;tga;gif;psd;hdr;pic;pnm" }, { "All files", "*" } }; // Флаги диалогового окна
    bool running (true); // Флаг работы программы

    // Кэши (чтобы уменьшить нагрузку)
    ImageGray img1, img2, imgDiff;
    std::array<int, 256> h1{}, h2{}, hd{};

    while (running) {
        while (SDL_PollEvent(&eventer)) {
            ImGui_ImplSDL3_ProcessEvent(&eventer);
            if (eventer.type == SDL_EVENT_QUIT) // Обработка события того, что приложение закрывается
                running = false;
        }

        if (dlg.ready) { // Диалог закрылся
            if (dlg.ok && loadImageRGB(dlg.path, image)) { // Файл выбран и успешно загружен

                // Освобождение старых текстур
                if (texImage) SDL_DestroyTexture(texImage);
                if (texImg1)  SDL_DestroyTexture(texImg1);
                if (texImg2)  SDL_DestroyTexture(texImg2);
                if (texDiff)  SDL_DestroyTexture(texDiff);

                img1 = toGray(image, true);
                img2 = toGray(image, false);
                imgDiff = diffGray(img1, img2);

                h1 = intensityHistogram(img1);
                h2 = intensityHistogram(img2);
                hd = intensityHistogram(imgDiff);

                texImage = makeTextureRGB(renderer, image);
                texImg1 = makeTextureGray(renderer, toGray(image, true));
                texImg2 = makeTextureGray(renderer, toGray(image, false));
                texDiff = makeTextureGray(renderer, diffGray(img1, img2));
            }
            dlg.reset(); // Сброс состояния диалогового окна
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Панель и кнопки
        ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20.f,
            io.DisplaySize.y - 20.f),
            ImGuiCond_Always);

        ImGui::Begin("Color Models", nullptr, ImGuiWindowFlags_NoCollapse);

        const float buttonPanelW = 260.f;
        ImVec2 contentAvail = ImGui::GetContentRegionAvail();

        // Левая область - отображение загруженного изображения
        ImGui::BeginChild("##image_area",
            ImVec2(contentAvail.x - buttonPanelW - 10.f, 0.f),
            true);

        if (!image.empty() && texImage) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float scale = std::min(avail.x / (float)image.width,
                avail.y / (float)image.height);
            ImVec2 sz((float)image.width * scale, (float)image.height * scale);

            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(pos.x + (avail.x - sz.x) * 0.5f,
                pos.y + (avail.y - sz.y) * 0.5f));

            ImGui::Image((ImTextureID)(intptr_t)texImage, sz);
        }
        else {
            ImGui::TextUnformatted("Load an image to see it here");
        }

        ImGui::EndChild();


        // Правая область - кнопки
        ImGui::SameLine();
        ImGui::BeginChild("##button_panel", ImVec2(buttonPanelW, 0.f), true);

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

        if (ImGui::Button("Task 1")) { activeTask = ActiveTask::Task1; showWindow2 = true; }
        if (ImGui::Button("Task 2")) { activeTask = ActiveTask::Task2; showWindow2 = true; }
        if (ImGui::Button("Task 3")) { activeTask = ActiveTask::Task3; showWindow2 = true; }

        ImGui::EndChild();
        ImGui::End();


        if (showWindow2) {
            ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);
            ImGui::Begin("Task 1", &showWindow2,
                ImGuiWindowFlags_HorizontalScrollbar);

            if (!image.empty() && activeTask == ActiveTask::Task1) {
                SDL_Texture* texs[3] = { texImg1, texImg2, texDiff };
                const std::array<int, 256>* hists[3] = { &h1, &h2, &hd };
                const char* labels[3] = {
                    "NTSC (0.299 / 0.587 / 0.114)",
                    "sRGB (0.2126 / 0.7152 / 0.0722)",
                    "Difference (normalized)"
                };
                ImU32 histColors[3] = {
                    IM_COL32(255, 255, 255, 255),
                    IM_COL32(255, 255, 255, 255),
                    IM_COL32(180, 180, 255, 255)
                };

                const float gap = 8.f;
                const float rowH = 20.f;   // высота строки с подписью
                const float sepH = 8.f;    // высота разделителя

                // Сколько всего доступно под содержимое
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float totalH = avail.y;
                float blockH = (totalH - 3.f * (rowH + sepH)) / 3.f;
                if (blockH < 40.f) blockH = 40.f;   // минимальная высота блока

                // Половина ширины — картинка, половина — гистограмма (минус gap)
                float blockW = (avail.x - gap) * 0.5f;
                if (blockW < 40.f) blockW = 40.f;

                ImVec2 imgSize(blockW, blockH);
                ImVec2 histSize(blockW, blockH);

                for (int k = 0; k < 3; ++k) {
                    ImGui::TextUnformatted(labels[k]);
                    ImGui::BeginGroup();

                    // Картинка с сохранением пропорций
                    {
                        float texW = (float)image.width;
                        float texH = (float)image.height;
                        float s = std::min(imgSize.x / texW, imgSize.y / texH);
                        ImVec2 drawSz(texW * s, texH * s);

                        // Центрирование внутри выделенного блока
                        ImVec2 p = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(ImVec2(
                            p.x + (imgSize.x - drawSz.x) * 0.5f,
                            p.y + (imgSize.y - drawSz.y) * 0.5f));

                        ImGui::Image((ImTextureID)(intptr_t)texs[k], drawSz);

                        // Возвращаем курсор под блок картинки
                        ImGui::SetCursorPos(ImVec2(p.x + imgSize.x, p.y));
                    }

                    ImGui::SameLine(0.f, gap);

                    // Гистограмма
                    {
                        ImVec2 histPos = ImGui::GetCursorScreenPos();
                        ImGui::Dummy(histSize);

                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        dl->AddRect(histPos,
                            ImVec2(histPos.x + histSize.x, histPos.y + histSize.y),
                            IM_COL32(120, 120, 120, 255));
                        drawHistImGui(dl, histPos, histSize, *hists[k], histColors[k]);
                    }

                    ImGui::EndGroup();
                    ImGui::Separator();
                }
            }
            else {
                ImGui::TextUnformatted("Load an image first");
            }

            ImGui::End();
        }        

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        
        SDL_RenderPresent(renderer);

    }
    if (texImage) SDL_DestroyTexture(texImage);
    if (texImg1)  SDL_DestroyTexture(texImg1);
    if (texImg2)  SDL_DestroyTexture(texImg2);
    if (texDiff)  SDL_DestroyTexture(texDiff);



    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}