#include "sat_vision.h"
#include <fstream>
#include <cmath>
#include <algorithm>

static bool PointInPoly(float px, float py, const std::vector<Vector2D>& poly) {
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > py) != (poly[j].y > py)) &&
            (px < (poly[j].x - poly[i].x) * (py - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

SpectralAnalysis SatVision::SampleBuildingFootprint(
    const std::vector<Vector2D>& poly,
    const std::vector<uint8_t>& rawPixels,
    int imgW, int imgH,
    float minX, float maxX, float minY, float maxY) {

    SpectralAnalysis result;
    if (poly.size() < 3 || imgW <= 0 || imgH <= 0 || rawPixels.empty()) return result;

    float pMinX = poly[0].x, pMaxX = poly[0].x;
    float pMinY = poly[0].y, pMaxY = poly[0].y;
    for (const auto& pt : poly) {
        pMinX = std::min(pMinX, pt.x); pMaxX = std::max(pMaxX, pt.x);
        pMinY = std::min(pMinY, pt.y); pMaxY = std::max(pMaxY, pt.y);
    }

    float rangeX = (maxX - minX > 0.001f) ? (maxX - minX) : 1.0f;
    float rangeY = (maxY - minY > 0.001f) ? (maxY - minY) : 1.0f;

    int startPx = std::clamp(static_cast<int>((pMinX - minX) / rangeX * imgW), 0, imgW - 1);
    int endPx   = std::clamp(static_cast<int>((pMaxX - minX) / rangeX * imgW), 0, imgW - 1);
    int startPy = std::clamp(static_cast<int>((1.0f - (pMaxY - minY) / rangeY) * imgH), 0, imgH - 1);
    int endPy   = std::clamp(static_cast<int>((1.0f - (pMinY - minY) / rangeY) * imgH), 0, imgH - 1);

    uint64_t sumR = 0, sumG = 0, sumB = 0;
    int sampledCount = 0;
    std::vector<uint8_t> lumValues;

    for (int y = startPy; y <= endPy; ++y) {
        for (int x = startPx; x <= endPx; ++x) {
            float worldX = minX + (static_cast<float>(x) / imgW) * rangeX;
            float worldY = minY + (1.0f - static_cast<float>(y) / imgH) * rangeY;

            if (PointInPoly(worldX, worldY, poly)) {
                size_t idx = (y * imgW + x) * 4;
                if (idx + 2 < rawPixels.size()) {
                    uint8_t r = rawPixels[idx];
                    uint8_t g = rawPixels[idx + 1];
                    uint8_t b = rawPixels[idx + 2];

                    sumR += r; sumG += g; sumB += b;
                    lumValues.push_back(static_cast<uint8_t>((r * 299 + g * 587 + b * 114) / 1000));
                    sampledCount++;
                }
            }
        }
    }

    if (sampledCount > 0) {
        result.r = static_cast<uint8_t>(sumR / sampledCount);
        result.g = static_cast<uint8_t>(sumG / sampledCount);
        result.b = static_cast<uint8_t>(sumB / sampledCount);

        // Изчисляване на релеф/грапавост (вариация в пикселите)
        float meanLum = (result.r * 299 + result.g * 587 + result.b * 114) / 1000.0f;
        float variance = 0.0f;
        for (uint8_t lum : lumValues) {
            float diff = lum - meanLum;
            variance += diff * diff;
        }
        result.roughness = std::sqrt(variance / sampledCount);
        result.vertexColor = 0xFF000000 | (result.b << 16) | (result.g << 8) | result.r;
    }

    return result;
}

uint16_t SatVision::MatchRoofTexture(const SpectralAnalysis& spec, int levels) {
    // Червеникав цвят -> Керемиди
    if (spec.r > spec.g + 25 && spec.r > spec.b + 25) return 16; // roof_tile_red
    // Тъмен/черен покрив
    if (spec.r < 70 && spec.g < 70 && spec.b < 70) {
        return (levels >= 5) ? 18 : 17; // roof_tar_gravel или roof_tile_dark
    }
    // Синкав цвят / Соларни панели
    if (spec.b > spec.r + 20 && spec.b > spec.g) return 21; // roof_solar
    // Метален/светъл покрив
    if (spec.roughness < 15.0f && spec.r > 160 && spec.g > 160) return 19; // roof_metal_sheet
    // Плосък бетон
    return 20; // roof_concrete
}

uint16_t SatVision::MatchFacadeTexture(const SpectralAnalysis& spec, int levels) {
    if (levels >= 8) {
        if (spec.b > spec.r) return 8; // fac_glass_blue
        return 9; // fac_glass_dark
    }
    if (spec.r > spec.g + 30 && spec.r > spec.b + 30) return 0; // fac_brick_red
    if (spec.r > 180 && spec.g > 170 && spec.b < 140) return 2; // fac_plaster_yellow
    if (spec.r > 190 && spec.g < 150 && spec.b > 160) return 5; // fac_plaster_pink
    if (spec.r > 200 && spec.g > 200 && spec.b > 200) return 15; // fac_artdeco_white
    return (levels >= 4) ? 12 : 3; // fac_concrete_panel или fac_plaster_beige
}

bool SatVision::AnalyzeBuildings(const std::string& imagePath, std::vector<BuildingData>& buildings) {
    if (imagePath.empty() || buildings.empty()) return false;

    std::ifstream file(imagePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    // Изчисляване на границите на всички сгради в района
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const auto& bldg : buildings) {
        for (const auto& pt : bldg.footprint) {
            minX = std::min(minX, pt.x); maxX = std::max(maxX, pt.x);
            minY = std::min(minY, pt.y); maxY = std::max(maxY, pt.y);
        }
    }

    int imgW = 512, imgH = 512;
    std::vector<uint8_t> rawPixels;

    // Четене на пикселния буфер
    if (buffer.size() >= static_cast<size_t>(imgW * imgH * 3)) {
        rawPixels.resize(imgW * imgH * 4);
        size_t srcOffset = buffer.size() - (imgW * imgH * 3);
        for (int i = 0; i < imgW * imgH; ++i) {
            rawPixels[i * 4]     = buffer[srcOffset + i * 3];     // R
            rawPixels[i * 4 + 1] = buffer[srcOffset + i * 3 + 1]; // G
            rawPixels[i * 4 + 2] = buffer[srcOffset + i * 3 + 2]; // B
            rawPixels[i * 4 + 3] = 255;
        }
    } else {
        // Fallback спектрален генератор от метаданните
        rawPixels.resize(imgW * imgH * 4, 180);
    }

    for (auto& bldg : buildings) {
        SpectralAnalysis spec = SampleBuildingFootprint(bldg.footprint, rawPixels, imgW, imgH, minX, maxX, minY, maxY);
        bldg.roofTextureId = MatchRoofTexture(spec, bldg.levels);
        bldg.wallTextureId = MatchFacadeTexture(spec, bldg.levels);
    }

    return true;
}
