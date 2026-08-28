#pragma once

#include "osm_parser.h"
#include <string>
#include <vector>
#include <cstdint>

struct SpectralAnalysis {
    uint8_t r = 180;
    uint8_t g = 180;
    uint8_t b = 180;
    float roughness = 0.0f;
    uint32_t vertexColor = 0xFFFFFFFF;
};

class SatVision {
public:
    static bool AnalyzeBuildings(
        const std::string& imagePath,
        std::vector<BuildingData>& buildings);

private:
    static SpectralAnalysis SampleBuildingFootprint(
        const std::vector<Vector2D>& poly,
        const std::vector<uint8_t>& rawPixels,
        int imgW, int imgH,
        float minX, float maxX, float minY, float maxY);

    static uint16_t MatchRoofTexture(const SpectralAnalysis& spec, int levels);
    static uint16_t MatchFacadeTexture(const SpectralAnalysis& spec, int levels);
};
