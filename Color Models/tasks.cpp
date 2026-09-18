


#include "tasks.h"
#include "dialog_windows.h"

const SDL_DialogFileFilter Task3::saveFilters[2] = { { "PNG", "png"}, { "All files", "*"} };
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
void Task1::prepare(SDL_Renderer* renderer, const ImageRGB& image) { // Выполняется 1 раз при 1 открытии Task 1
    freeTextures();

    img1 = RGBtoGray(image, true);
    img2 = RGBtoGray(image, false);
    imgDiff = diffGray(img1, img2);

    h1 = intensityHistogram(img1);
    h2 = intensityHistogram(img2);
    hd = intensityHistogram(imgDiff);

    texImg1 = makeTextureGray(renderer, img1);
    texImg2 = makeTextureGray(renderer, img2);
    texDiff = makeTextureGray(renderer, imgDiff);
}
// Отрисовка Task1
void Task1::draw(const ImageRGB& image) const {
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

        float texW = (float)image.width;
        float texH = (float)image.height;
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
        drawHist(dl, histPos, histSize, *hists[k], histColors[k]);

        ImGui::EndGroup();
        ImGui::Separator();
    }
}

void Task2::prepare(SDL_Renderer* /*renderer*/, const ImageRGB& /*image*/) {}
void Task2::draw(const ImageRGB& /*image*/) const { // TODO
}

Task3::~Task3() noexcept { freeTextures(); }
void Task3::freeTextures() noexcept { if (texApplyed) { SDL_DestroyTexture(texApplyed); texApplyed = nullptr; } }
void Task3::prepare(SDL_Renderer* /*renderer*/, const ImageRGB& image) {
    freeTextures();
    applyed.data.clear();
    applyedPreview.data.clear();
    
    hsv = RGBtoHSV(image);
    ImageRGB preview = downscale(image, 512);
    hsvPreview = RGBtoHSV(preview);
    
    hueShift = 0;
    satScale = valScale = 1;
    lastH = lastS = lastV = 1e9;
}

void Task3::draw(SDL_Renderer* renderer, SDL_Window* window, const ImageRGB& image, SDL_Texture* texOriginal) {
    if (image.empty() || hsv.empty() || hsvPreview.empty()) {
        ImGui::TextUnformatted("Load an image first");
        return;
    }

    ImGui::TextUnformatted("HSV correction:");
    
    // Ползунки для HSV
    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Hue shift (degrees)", &hueShift, -180, 180, "%.0f");
    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Saturation scale", &satScale, 0, 2, "%.2f");
    ImGui::SetNextItemWidth(400);
    ImGui::SliderFloat("Value scale", &valScale, 0, 2, "%.2f");
    
    if (ImGui::Button("Reset sliders")) {
        hueShift = 0;
        satScale = valScale = 1;
    }

    ImGui::SameLine();
    
    if (ImGui::Button("Save as PNG...")) { // Полное разрешение — считаем один раз при нажатии
        applyed = applyHSV(hsv, hueShift, satScale, valScale);
        
        if (!saveDlg.pending) {
            saveDlg.pending = true;
            SDL_ShowSaveFileDialog(onSaveFileDialogResult, &saveDlg, window, saveFilters, 2, nullptr);
            }
        }
    ImGui::Separator();
    
    if (hueShift != lastH || satScale != lastS || valScale != lastV) {
        applyedPreview = applyHSV(hsvPreview, hueShift, satScale, valScale);

        if (texApplyed) SDL_DestroyTexture(texApplyed);
        texApplyed = makeTextureRGB(renderer, applyedPreview);
        
        lastH = hueShift;
        lastS = satScale;
        lastV = valScale;
    }
    
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float halfW = (avail.x - 10) * 0.5;
    float imgH = avail.y - 10;
    if (halfW < 20) halfW = 20;
    if (imgH < 20) imgH = 20;
    
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Original");
    if (texOriginal) {
        float s(std::min(halfW / image.width, imgH / image.height));
        ImGui::Image((ImTextureID)(intptr_t)texOriginal, ImVec2(image.width * s, image.height * s));
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
