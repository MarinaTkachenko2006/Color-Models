


#include "tasks.h"
#include "dialog_windows.h"

Task1::~Task1() noexcept { freeTextures(); }
// Уничтожает все три текстуры и обнуляет указатели
void Task1::freeTextures() noexcept {
    if (texImg1) { SDL_DestroyTexture(texImg1); texImg1 = nullptr; }
    if (texImg2) { SDL_DestroyTexture(texImg2); texImg2 = nullptr; }
    if (texDiff) { SDL_DestroyTexture(texDiff); texDiff = nullptr; }
}
// Перевод в оттенки серого двумя формулами
// Вычисление разности
// Вычисление трёх гистограмм
// Создание трёх текстур
void Task1::prepare(const AppContext& ctx) { // Выполняется 1 раз при 1 открытии Task 1
    if (!ctx.renderer || !ctx.image || ctx.image->empty()) return;

    freeTextures();

    const ImageRGB& image = *ctx.image;

    img1 = RGBtoGray(image, true);
    img2 = RGBtoGray(image, false);
    imgDiff = diffGray(img1, img2);

    h1 = intensityHistogram(img1);
    h2 = intensityHistogram(img2);
    hd = intensityHistogram(imgDiff);

    texImg1 = makeTextureGray(ctx.renderer, img1);
    texImg2 = makeTextureGray(ctx.renderer, img2);
    texDiff = makeTextureGray(ctx.renderer, imgDiff);
}
// Отрисовка Task1
void Task1::drawByTexture(const AppContext& ctx) {
    if (!ctx.image) return;

    const ImageRGB& image = *ctx.image;

    SDL_Texture* texs[3] = { texImg1, texImg2, texDiff };
    const std::array<int, 256>* hists[3] = { &h1, &h2, &hd };
    const char* labels[3] = {
        "NTSC (0.299 / 0.587 / 0.114)",
        "sRGB (0.2126 / 0.7152 / 0.0722)",
        "Difference (normalized)" };
    ImU32 histColors[3] = {
        IM_COL32(255, 255, 255, 255),
        IM_COL32(255, 255, 255, 255),
        IM_COL32(180, 180, 255, 255) };

    const float gap = 8.0f;
    const float rowH = 20.0f;
    const float sepH = 8.0f;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float blockH = (avail.y - 3.0f * (rowH + sepH)) / 3.0f;
    if (blockH < 40.0f) blockH = 40.0f;

    float blockW = (avail.x - gap) * 0.5f;
    if (blockW < 40.0f) blockW = 40.0f;

    ImVec2 imgSize(blockW, blockH);
    ImVec2 histSize(blockW, blockH);

    for (int k = 0; k < 3; ++k) {
        ImGui::TextUnformatted(labels[k]);
        ImGui::BeginGroup();

        float texW = image.width;
        float texH = image.height;
        float s = std::min(imgSize.x / texW, imgSize.y / texH);
        ImVec2 drawSz(texW * s, texH * s);

        ImVec2 p = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(p.x + (imgSize.x - drawSz.x) * 0.5, p.y + (imgSize.y - drawSz.y) * 0.5));

        ImGui::Image((ImTextureID)(intptr_t)texs[k], drawSz);

        ImGui::SetCursorPos(ImVec2(p.x + imgSize.x, p.y));

        ImGui::SameLine(0, gap);
        ImVec2 histPos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(histSize);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(histPos, ImVec2(histPos.x + histSize.x, histPos.y + histSize.y), IM_COL32(120, 120, 120, 255));
        drawHistogramByTexture(dl, histPos, histSize, *hists[k], histColors[k]);

        ImGui::EndGroup();
        ImGui::Separator();
    }
}

void Task1::drawByPixels(const AppContext& ctx) {
    if (!ctx.renderer) return;
    if (img1.empty() || img2.empty() || imgDiff.empty()) return;

    SDL_Renderer* renderer = ctx.renderer;
    float originX = ctx.originX;
    float originY = ctx.originY;
    float areaW = ctx.areaW;
    float areaH = ctx.areaH;

    SDL_Rect clip{ (int)originX, (int)originY, (int)areaW, (int)areaH };
    SDL_SetRenderClipRect(renderer, &clip);

    const float gap = 8.0f;   // зазор между картинкой и гистограммой
    const float rowH = 20.0f;  // резерв под подпись
    const float sepH = 8.0f;   // высота разделителя

    float blockH = (areaH - 3 * (rowH + sepH)) / 3;
    if (blockH < 40) blockH = 40;

    float blockW = (areaW - gap) * 0.5;
    if (blockW < 40) blockW = 40;

    const ImageGray* imgs[3] = { &img1, &img2, &imgDiff };
    const std::array<int, 256>* hists[3] = { &h1, &h2, &hd };

    for (int k = 0; k < 3; ++k) {
        float blockTop = originY + k * (blockH + rowH + sepH);

        // Отрисовка левого пространства - изображения
        const ImageGray& im = *imgs[k];

        float s = std::min(blockW / (float)im.width,
            blockH / (float)im.height);
        float dw = im.width * s;
        float dh = im.height * s;
        float dx = originX + (blockW - dw) * 0.5f;
        float dy = blockTop + (blockH - dh) * 0.5f;

        for (int py = 0; py < (int)dh; ++py) {
            int sy = (int)(py / s);
            if (sy >= im.height) sy = im.height - 1;

            for (int px = 0; px < (int)dw; ++px) {
                int sx = (int)(px / s);
                if (sx >= im.width) sx = im.width - 1;

                uint8_t v = *im.at(sx, sy);
                SDL_SetRenderDrawColor(renderer, v, v, v, 255);
                SDL_RenderPoint(renderer, dx + px, dy + py);
            }
        }

        // Отрисовка правого пространства - гистограммы
        const std::array<int, 256>& hgt = *hists[k];

        int maxH = 0;
        for (int v : hgt) if (v > maxH) maxH = v;
        if (maxH <= 0) continue;

        float scale = blockH / (float)maxH;
        float hx = originX + blockW + gap;
        float dxh = blockW / 256.0f;

        Uint8 cr = 255, cg = 255, cb = 255;
        if (k == 2) { cr = 180; cg = 180; cb = 255; }
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);

        for (int v = 0; v < 256; ++v) {
            float xv = hx + (v + 0.5f) * dxh;
            float barH = hgt[v] * scale;
            if (barH < 1.0f && hgt[v] > 0) barH = 1.0f;

            for (int py = 0; py < (int)barH; ++py)
                SDL_RenderPoint(renderer, xv, blockTop + blockH - py);
        }
    }

    SDL_SetRenderClipRect(renderer, nullptr);
}

const SDL_DialogFileFilter Task3::saveFilters[2] = { { "PNG", "png"}, { "All files", "*"} };

Task3::~Task3() noexcept { freeTextures(); }
void Task3::freeTextures() noexcept { if (texApplyed) { SDL_DestroyTexture(texApplyed); texApplyed = nullptr; } }
void Task3::prepare(const AppContext& ctx) {
    if (!ctx.image || ctx.image->empty()) return;

    freeTextures();
    applyed.data.clear();
    applyedPreview.data.clear();

    const ImageRGB& image = *ctx.image;
    
    hsv = RGBtoHSV(image);
    ImageRGB preview = downscale(image, 512);
    hsvPreview = RGBtoHSV(preview);
    
    hueShift = 0;
    satScale = valScale = 1;
    lastH = lastS = lastV = 1e9;
}

void Task3::updatePreview(SDL_Renderer* renderer) {
    if (!renderer) return;
    if (hsvPreview.empty()) return;

    if (hueShift != lastH || satScale != lastS || valScale != lastV) {
        applyedPreview = applyHSV(hsvPreview, hueShift, satScale, valScale);

        if (texApplyed) SDL_DestroyTexture(texApplyed);
        texApplyed = makeTextureRGB(renderer, applyedPreview);

        lastH = hueShift;
        lastS = satScale;
        lastV = valScale;
    }
}


void Task3::drawByTexture(const AppContext& ctx) {
    if (!ctx.renderer || !ctx.window || !ctx.image || ctx.image->empty()) {
        ImGui::TextUnformatted("Load an image first");
        return;
    }

    const ImageRGB& image = *ctx.image;
    
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float halfW = (avail.x - 10) * 0.5;
    float imgH = avail.y - 10;
    if (halfW < 20) halfW = 20;
    if (imgH < 20) imgH = 20;
    
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Original");
    if (ctx.texImage) {
        float s(std::min(halfW / image.width, imgH / image.height));
        ImGui::Image((ImTextureID)(intptr_t)ctx.texImage, ImVec2(image.width * s, image.height * s));
    }
    
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Applyed (HSV)");

    if (texApplyed && !applyedPreview.empty()) {
        float s (std::min(halfW / applyedPreview.width, imgH / applyedPreview.height));
        ImGui::Image((ImTextureID)(intptr_t)texApplyed, ImVec2(applyedPreview.width * s, applyedPreview.height * s));
    }

    ImGui::EndGroup();
}

void Task3::drawByPixels(const AppContext& ctx) {
    if (!ctx.renderer || !ctx.image || ctx.image->empty()) return;

    SDL_Renderer* renderer = ctx.renderer;
    const ImageRGB& image = *ctx.image;

    if (hsv.empty() || hsvPreview.empty()) return;
    if (ctx.areaW < 20.0f || ctx.areaH < 20.0f) return;

    SDL_Rect clip{ (int)ctx.originX, (int)ctx.originY, (int)ctx.areaW, (int)ctx.areaH };
    SDL_SetRenderClipRect(renderer, &clip);

    // Пересчёт превью сделан в updatePreview(), здесь только отрисовка.
    const float gap = 10.0f;
    float halfW = (ctx.areaW - gap) * 0.5f;
    float imgH = ctx.areaH;

    const ImageRGB* imgs[2] = { &image, &applyedPreview };

    for (int k = 0; k < 2; ++k) {
        const ImageRGB& im = *imgs[k];
        if (im.empty()) continue;

        float s = std::min(halfW / (float)im.width,
            imgH / (float)im.height);
        float dw = im.width * s;
        float dh = im.height * s;

        float dx = ctx.originX + k * (halfW + gap);
        float dy = ctx.originY + (imgH - dh) * 0.5f;

        for (int py = 0; py < (int)dh; ++py) {
            int sy = (int)(py / s);
            if (sy >= im.height) sy = im.height - 1;

            for (int px = 0; px < (int)dw; ++px) {
                int sx = (int)(px / s);
                if (sx >= im.width) sx = im.width - 1;

                const uint8_t* p = im.at(sx, sy);
                SDL_SetRenderDrawColor(renderer, p[0], p[1], p[2], 255);
                SDL_RenderPoint(renderer, dx + px, dy + py);
            }
        }
    }
    SDL_SetRenderClipRect(renderer, nullptr);
}

void Task3::drawControls(const AppContext& ctx)
{
    if (hsv.empty() || hsvPreview.empty()) return;

    ImGui::TextUnformatted("HSV correction:");

    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Hue shift (degrees)", &hueShift, -180.0f, 180.0f, "%.0f");
    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Saturation scale", &satScale, 0.0f, 2.0f, "%.2f");
    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Value scale", &valScale, 0.0f, 2.0f, "%.2f");

    if (ImGui::Button("Reset sliders")) {
        hueShift = 0.0f;
        satScale = 1.0f;
        valScale = 1.0f;
    }

    ImGui::SameLine();

    if (ImGui::Button("Save as PNG...")) {
        // Полное разрешение — считаем один раз по нажатию
        applyed = applyHSV(hsv, hueShift, satScale, valScale);

        if (!saveDlg.pending) {
            saveDlg.pending = true;
            SDL_ShowSaveFileDialog(onSaveFileDialogResult, &saveDlg,
                ctx.window, saveFilters, 2, nullptr);
        }
    }

    ImGui::Separator();
}
