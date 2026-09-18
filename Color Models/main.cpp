#define _CRT_SECURE_NO_WARNINGS

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
#include <array>
#include <algorithm>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static bool showWindow2 = false; // Флаг открытости 2го окна
enum class ActiveTask { None, Task1, Task2, Task3 };
struct PixelHSV { float h = 0, s = 0, v = 0; };  // h = [0;360); s,v =[0;1)
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event eventer;

// ---- Структуры диалогов ----

// Состояние диалога выбора файла
struct FileDialogState
{
    bool pending = false; // true, пока диалог открыт и ответа ещё нет
    bool ready = false; // true, когда колбэк сработал — можно обрабатывать результат
    bool ok = false; // true — пользователь выбрал файл; false — нажал «Отмена»
    std::string path; // путь к выбранному файлу (валиден только при ok == true)

    void reset() noexcept { ready = false; ok = false; path.clear(); }
};

// Состояние диалога сохранения файла
struct SaveDialogState
{
    bool pending = false;  // true, пока диалог сохранения открыт
    bool ready = false;  // true, когда колбэк сработал — можно обрабатывать результат
    bool ok = false;  // true — пользователь задал имя файла; false — отмена
    std::string path;      // путь, куда сохранять (валиден только при ok == true)

    void reset() noexcept { ready = false; ok = false; path.clear(); }
};

// ---- Классы представлений изображений ----

class ImageRGB {
public:
    int width = 0; // Ширина изображения
    int height = 0; // Высота изображения
    int channels = 0; // Количество каналов
    std::vector<uint8_t> data; // Вектор битов размера width * height * 3

    ImageRGB() noexcept = default;

    bool empty() const noexcept { return data.empty(); }
    uint8_t* at(int x, int y) { return &data[(static_cast<size_t>(y) * width + x) * 3]; }
    const uint8_t* at(int x, int y) const { return &data[(static_cast<size_t>(y) * width + x) * 3]; }
};

class ImageGray {
public:
    int width = 0; // Ширина изображения
    int height = 0; // Высота изображения
    int channels = 0; // Количество каналов
    std::vector<uint8_t> data; // Вектор битов размера width * height

    ImageGray() noexcept = default;

    bool empty() const noexcept { return data.empty(); }
    uint8_t* at(int x, int y) { return &data[(static_cast<size_t>(y) * width + x)]; }
    const uint8_t* at(int x, int y) const { return &data[(static_cast<size_t>(y) * width + x)]; }
};

class ImageHSV
{
public:
    int width = 0, height = 0; // Ширина и высота изображения
    std::vector<PixelHSV> data; // Вектор троек HSV

    ImageHSV() noexcept = default;

    bool empty() const noexcept { return data.empty(); }
    PixelHSV& at(int x, int y) { return data[static_cast<size_t>(y) * width + x]; }
    const PixelHSV& at(int x, int y) const { return data[static_cast<size_t>(y) * width + x]; }
};


// ---- Методы для диалогов ----

static void SDLCALL onFileDialogResult(void* userdata,
    const char* const* filelist,
    int /*filter*/)
{
    auto* st = static_cast<FileDialogState*>(userdata);

    // Диалог закрыт, результат готов к обработке главным циклом
    st->pending = false;
    st->ready = true;

    // Файл не выбран, т.е. пользователь нажал "отмена"
    if (!filelist || !filelist[0]) { st->ok = false; return; }

    // Файл выбран
    st->ok = true;
    st->path = filelist[0];
}

static void SDLCALL onSaveFileDialogResult(void* userdata,
    const char* const* filelist,
    int /*filter*/)
{
    auto* st = static_cast<SaveDialogState*>(userdata);

    // Диалог закрыт, результат готов к обработке
    st->pending = false;
    st->ready = true;

    // Пользователь не задал путь, т.е. пользователь нажал "отмена"
    if (!filelist || !filelist[0]) { st->ok = false; return; }

    // Путь получен
    st->ok = true;
    st->path = filelist[0];
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

// ---- Методы отрисовки изображений ----

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

// ---- Методы преобразований изображений ----

// Преобразование RGB-изображения в Gray-изображение
ImageGray RGBtoGray(const ImageRGB& img, bool formula) {
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
    else // sRGB
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

// Преобразование RGB-изображения в HSV-изображение
void RGBconvertHSV(uint8_t R, uint8_t G, uint8_t B, float& H, float& S, float& V)
{
    float NR(R / 255.f), NG(G / 255.f), NB(B / 255.f);

    float MAX(std::max({ NR, NG, NB })), MIN(std::min({ NR, NG, NB }));
    float diff(MAX - MIN);

    V = MAX;
    S = (MAX == 0) ? 0 : (1 - MIN / MAX);
    if (MAX == MIN) H = 0;
    else if (MAX == NR && NG >= NB) H = 60 * (NG - NB) / diff;
    else if (MAX == NR && NG < NB) H = 60 * (NG - NB) / diff + 360;
    else if (MAX == NG) H = 60 * (NB - NR) / diff + 120;
    else H = 60 * (NR - NG) / diff + 240;
}

// Преобразование HSV-изображения в RGB-изображение
void HSVconvertRGB(float H, float S, float V, uint8_t& R, uint8_t& G, uint8_t& B)
{
    int Hi = static_cast<int>(std::floor(H / 60.f)) % 6;
    float f = H / 60.f - std::floor(H / 60.f);

    float p = V * (1.f - S);
    float q = V * (1.f - f * S);
    float t = V * (1.f - (1.f - f) * S);

    float NR = 0.f, NG = 0.f, NB = 0.f;
    switch (Hi) {
    case 0:
        NR = V;
        NG = t;
        NB = p;
        break;
    case 1:
        NR = q;
        NG = V;
        NB = p;
        break;
    case 2:
        NR = p;
        NG = V;
        NB = t;
        break;
    case 3:
        NR = p;
        NG = q;
        NB = V;
        break;
    case 4:
        NR = t;
        NG = p;
        NB = V;
        break;
    case 5:
        NR = V;
        NG = p;
        NB = q;
        break;
    }

    R = static_cast<uint8_t>(std::clamp(NR * 255.0f + 0.5f, 0.0f, 255.0f));
    G = static_cast<uint8_t>(std::clamp(NG * 255.0f + 0.5f, 0.0f, 255.0f));
    B = static_cast<uint8_t>(std::clamp(NB * 255.0f + 0.5f, 0.0f, 255.0f));
}

// RGB-изображение → HSV-изображение
ImageHSV RGBtoHSV(const ImageRGB& img)
{
    ImageHSV out;
    out.width = img.width;
    out.height = img.height;
    out.data.resize(static_cast<size_t>(img.width) * img.height);

    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* p = img.at(x, y);
            PixelHSV& px = out.at(x, y);
            RGBconvertHSV(p[0], p[1], p[2], px.h, px.s, px.v);
        }
    return out;
}

// Цветокоррекция в пространстве HSV с возвращением получившегося RGB-изображения
ImageRGB applyHSV(const ImageHSV& hsv, float hueShift, float satScale, float valScale)
{
    ImageRGB out;
    out.width = hsv.width;
    out.height = hsv.height;
    out.channels = 3;
    out.data.resize(static_cast<size_t>(hsv.width) * hsv.height * 3);

    for (size_t i = 0; i < hsv.data.size(); ++i) {
        const PixelHSV& p = hsv.data[i];

        float H = p.h + hueShift;
        while (H < 0)    H += 360;
        while (H >= 360) H -= 360;

        float S (std::clamp(p.s * satScale, 0.0f, 1.0f)), V (std::clamp(p.v * valScale, 0.0f, 1.0f));

        uint8_t R, G, B;
        HSVconvertRGB(H, S, V, R, G, B);

        out.data[i * 3 + 0] = R;
        out.data[i * 3 + 1] = G;
        out.data[i * 3 + 2] = B;
    }
    return out;
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
        if (barH < 1 && hgt[v] > 0) barH = 1;

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
        if (barH < 1 && hgt[v] > 0) barH = 1;
        dl->AddLine(ImVec2(xv, baseY), ImVec2(xv, baseY - barH), color);
    }
}

// Уменьшение изображения (для быстрого превью Task 3)
ImageRGB downscale(const ImageRGB& src, int maxSide)
{
    if (src.width <= maxSide && src.height <= maxSide) return src;

    float scale = std::min((float)maxSide / src.width,
        (float)maxSide / src.height);
    int nw = std::max(1, (int)(src.width * scale));
    int nh = std::max(1, (int)(src.height * scale));

    ImageRGB dst;
    dst.width = nw;
    dst.height = nh;
    dst.channels = 3;
    dst.data.resize((size_t)nw * nh * 3);

    for (int y = 0; y < nh; ++y) {
        int sy = (int)((float)y * src.height / nh);
        for (int x = 0; x < nw; ++x) {
            int sx = (int)((float)x * src.width / nw);
            const uint8_t* p = src.at(sx, sy);
            uint8_t* q = &dst.data[((size_t)y * nw + x) * 3];
            q[0] = p[0]; q[1] = p[1]; q[2] = p[2];
        }
    }
    return dst;
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

// ---- Методы выполнения заданий ----

// Задание 1
void drawTask1(const ImageRGB& image,
    SDL_Texture* texImg1, SDL_Texture* texImg2, SDL_Texture* texDiff,
    const std::array<int, 256>& h1,
    const std::array<int, 256>& h2,
    const std::array<int, 256>& hd)
{
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
    const float rowH = 20;   // высота строки с подписью
    const float sepH = 8.f;    // высота разделителя

    // Сколько всего доступно под содержимое
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float totalH = avail.y;
    float blockH = (totalH - 3.f * (rowH + sepH)) / 3.f;
    if (blockH < 40) blockH = 40;   // минимальная высота блока

    // Половина ширины — картинка, половина — гистограмма (минус gap)
    float blockW = (avail.x - gap) * 0.5f;
    if (blockW < 40) blockW = 40;

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

        ImGui::SameLine(0, gap);

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

// Задание 3
void drawTask3(SDL_Renderer* renderer,
    SDL_Window* window,
    const ImageRGB& image,
    SDL_Texture* texOriginal,
    const ImageHSV& hsv,
    const ImageHSV& hsvPreview,   // уменьшенный — для интерактивного превью
    ImageRGB& applyed,           // полный результат (заполняется при Save)
    ImageRGB& applyedPreview,    // уменьшенный результат (каждый пересчёт)
    SDL_Texture*& texapplyed,
    float& hueShift, float& satScale, float& valScale,
    float& lastH, float& lastS, float& lastV,
    SaveDialogState& saveDlg,
    const SDL_DialogFileFilter* saveFilters,
    int saveFilterCount)
{
    if (image.empty() || hsv.empty() || hsvPreview.empty()) {
        ImGui::TextUnformatted("Load an image first");
        return;
    }

    // ----- Слайдеры -----
    ImGui::TextUnformatted("HSV correction:");

    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Hue shift (deg)", &hueShift, -180, 180, "%.0f");
    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Saturation scale", &satScale, 0, 2.f, "%.2f");
    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Value scale", &valScale, 0, 2.f, "%.2f");

    if (ImGui::Button("Reset sliders")) {
        hueShift = 0;
        satScale = 1;
        valScale = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save as PNG...")) {
        // Полное разрешение — считаем ОДИН РАЗ при нажатии
        applyed = applyHSV(hsv, hueShift, satScale, valScale);

        if (!saveDlg.pending) {
            saveDlg.pending = true;
            SDL_ShowSaveFileDialog(onSaveFileDialogResult, &saveDlg,
                window, saveFilters, saveFilterCount,
                nullptr);
        }
    }
    ImGui::Separator();

    // Пересчёт оригинала
    bool needRecalc =
        hueShift != lastH ||
        satScale != lastS ||
        valScale != lastV;

    if (needRecalc) {
        applyedPreview = applyHSV(hsvPreview, hueShift, satScale, valScale);

        if (texapplyed) SDL_DestroyTexture(texapplyed);
        texapplyed = makeTextureRGB(renderer, applyedPreview);

        lastH = hueShift;
        lastS = satScale;
        lastV = valScale;
    }

    // Слева оригинал, справа результат
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float halfW = (avail.x - 10) * 0.5f;
    float imgH = avail.y - 10;
    if (halfW < 20) halfW = 20;
    if (imgH < 20) imgH = 20;

    ImGui::BeginGroup();
    ImGui::TextUnformatted("Original");
    if (texOriginal) {
        float s = std::min(halfW / (float)image.width,
            imgH / (float)image.height);
        ImGui::Image((ImTextureID)(intptr_t)texOriginal,
            ImVec2(image.width * s, image.height * s));
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::TextUnformatted("applyed (HSV)");
    if (texapplyed && !applyedPreview.empty()) {
        float s = std::min(halfW / (float)applyedPreview.width,
            imgH / (float)applyedPreview.height);
        ImGui::Image((ImTextureID)(intptr_t)texapplyed,
            ImVec2(applyedPreview.width * s, applyedPreview.height * s));
    }
    ImGui::EndGroup();
}

// Сохранение в PNG
bool saveImagePNG(const std::string& path, const ImageRGB& img)
{
    if (img.empty()) return false;
    return stbi_write_png(path.c_str(), img.width, img.height, 3,
        img.data.data(), img.width * 3) != 0;
}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init is failed" << std::endl;
        return -1;
    }

    window = SDL_CreateWindow("ImGui Window", 1000, 700, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow is failed" << std::endl;
        return 1;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
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

    ImageRGB image;
    SDL_Texture* texImage = nullptr;
    SDL_Texture* texImg1 = nullptr;
    SDL_Texture* texImg2 = nullptr;
    SDL_Texture* texDiff = nullptr;

    // ---- Task 3 ----
    ImageHSV hsv3;
    ImageHSV hsv3Preview;
    ImageRGB applyed3;
    ImageRGB applyed3Preview;
    SDL_Texture* texapplyed3 = nullptr;
    float hueShift3 = 0, satScale3 = 1, valScale3 = 1;
    float lastH3 = 1e9f, lastS3 = 1e9f, lastV3 = 1e9f;
    SaveDialogState saveDlg3;

    const SDL_DialogFileFilter saveFilters[] = {
        { "PNG",       "png" },
        { "All files", "*"   }
    };

    FileDialogState dlg; // Диалоговое окно
    ActiveTask activeTask = ActiveTask::None;
    const SDL_DialogFileFilter filters[] = { { "Images", "png;jpg;jpeg;bmp;tga;gif;psd;hdr;pic;pnm" }, { "All files", "*" } }; // Флаги диалогового окна
    bool running(true); // Флаг работы программы

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

                img1 = RGBtoGray(image, true);
                img2 = RGBtoGray(image, false);
                imgDiff = diffGray(img1, img2);

                h1 = intensityHistogram(img1);
                h2 = intensityHistogram(img2);
                hd = intensityHistogram(imgDiff);

                texImage = makeTextureRGB(renderer, image);
                texImg1 = makeTextureGray(renderer, RGBtoGray(image, true));
                texImg2 = makeTextureGray(renderer, RGBtoGray(image, false));
                texDiff = makeTextureGray(renderer, diffGray(img1, img2));

                // ---- Task 3 ----
                hsv3 = RGBtoHSV(image);

                ImageRGB imagePreview = downscale(image, 512);   // уменьшенная копия
                hsv3Preview = RGBtoHSV(imagePreview);

                if (texapplyed3) SDL_DestroyTexture(texapplyed3);
                texapplyed3 = nullptr;
                applyed3.data.clear();
                applyed3Preview.data.clear();
                hueShift3 = 0;
                satScale3 = 1;
                valScale3 = 1;
                lastH3 = lastS3 = lastV3 = 1e9f;
            }
            dlg.reset(); // Сброс состояния диалогового окна
        }

        if (saveDlg3.ready) {
            if (saveDlg3.ok && !applyed3.empty()) {
                if (!saveImagePNG(saveDlg3.path, applyed3))
                    std::cerr << "Save PNG failed: " << saveDlg3.path << std::endl;
            }
            saveDlg3.reset();
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Панель и кнопки
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20,
            io.DisplaySize.y - 20),
            ImGuiCond_Always);

        ImGui::Begin("Color Models", nullptr, ImGuiWindowFlags_NoCollapse);

        const float buttonPanelW = 260;
        ImVec2 contentAvail = ImGui::GetContentRegionAvail();

        // Левая область - отображение загруженного изображения
        ImGui::BeginChild("##image_area", ImVec2(contentAvail.x - buttonPanelW - 10, 0), true);

        if (!image.empty() && texImage) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float scale = std::min(avail.x / (float)image.width, avail.y / (float)image.height);
            ImVec2 sz((float)image.width * scale, (float)image.height * scale);

            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(pos.x + (avail.x - sz.x) * 0.5f,  pos.y + (avail.y - sz.y) * 0.5f));

            ImGui::Image((ImTextureID)(intptr_t)texImage, sz);
        }
        else {
            ImGui::TextUnformatted("Load an image to see it here");
        }

        ImGui::EndChild();

        // Правая область - кнопки
        ImGui::SameLine();
        ImGui::BeginChild("##button_panel", ImVec2(buttonPanelW, 0), true);

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
            const char* title =
                activeTask == ActiveTask::Task1 ? "Task 1" :
                activeTask == ActiveTask::Task2 ? "Task 2" :
                activeTask == ActiveTask::Task3 ? "Task 3" : "None";

            ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);
            ImGui::Begin(title, &showWindow2, ImGuiWindowFlags_HorizontalScrollbar);

            switch (activeTask) {
            case ActiveTask::Task1:
                if (!image.empty())
                    drawTask1(image, texImg1, texImg2, texDiff, h1, h2, hd);
                else
                    ImGui::TextUnformatted("Load an image first");
                break;

            case ActiveTask::Task2:
                break;

            case ActiveTask::Task3:
                drawTask3(renderer, window,
                    image, texImage,
                    hsv3, hsv3Preview,
                    applyed3, applyed3Preview,
                    texapplyed3,
                    hueShift3, satScale3, valScale3,
                    lastH3, lastS3, lastV3,
                    saveDlg3,
                    saveFilters,
                    static_cast<int>(sizeof(saveFilters) / sizeof(saveFilters[0])));
                break;

            case ActiveTask::None:
            default:
                ImGui::TextUnformatted("No task selected");
                break;
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
    if (texapplyed3) SDL_DestroyTexture(texapplyed3);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}