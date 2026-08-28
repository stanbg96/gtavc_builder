#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <cstdint>

struct Vector2D {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vector3D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct BuildingData {
    int64_t id = 0;
    std::vector<Vector2D> footprint;
    float height = 9.0f;
    int levels = 3;
    std::string buildingType = "yes";
    std::string material = "";
    std::string colour = "";
    std::string roofMaterial = "";
    std::string roofColour = "";
    std::string shopType = "";
    uint16_t wallTextureId = 0;
    uint16_t roofTextureId = 16;
};

struct RoadSegment {
    int64_t id = 0;
    std::vector<Vector2D> points;
    float width = 6.0f;
    std::string highwayType = "residential";
    uint16_t roadTextureId = 23;
};

struct TerrainPolygon {
    int64_t id = 0;
    std::vector<Vector2D> points;
    std::string terrainType;
    uint16_t terrainTextureId = 26;
};

struct MapChunk {
    int chunkX = 0;
    int chunkY = 0;
    std::string chunkName;
    std::vector<BuildingData> buildings;
    std::vector<RoadSegment> roads;
    std::vector<TerrainPolygon> terrain;
};

class OsmParser {
public:
    OsmParser();
    ~OsmParser();

    bool ParseStream(const std::string& filePath, std::function<void(float)> progressCb);
    const std::vector<MapChunk>& GetChunks() const { return chunks_; }
    size_t GetTotalBuildings() const { return total_buildings_; }

private:
    void SetOrigin(double lat, double lon);
    Vector2D ProjectLatLon(double lat, double lon) const;
    void PartitionIntoChunks(float chunkSizeMeters = 250.0f);
    void AnalyzeBuildingTextures(BuildingData& bldg);
    
    static std::string ExtractAttr(const std::string& line, const std::string& attr);
    static float ParseHeight(const std::string& heightStr, const std::string& levelsStr);

    double origin_lat_ = 0.0;
    double origin_lon_ = 0.0;
    bool has_origin_ = false;

    std::unordered_map<int64_t, Vector2D> node_lookup_;
    std::vector<BuildingData> parsed_buildings_;
    std::vector<RoadSegment> parsed_roads_;
    std::vector<TerrainPolygon> parsed_terrain_;
    std::vector<MapChunk> chunks_;
    size_t total_buildings_ = 0;
};
