#include "osm_parser.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <map>

static constexpr double PI = 3.14159265358979323846;
static constexpr double DEG2RAD = PI / 180.0;
static constexpr double EARTH_RADIUS = 6378137.0;

OsmParser::OsmParser() = default;
OsmParser::~OsmParser() = default;

std::string OsmParser::ExtractAttr(const std::string& line, const std::string& attr) {
    std::string pattern = attr + "=\"";
    size_t start = line.find(pattern);
    if (start == std::string::npos) {
        pattern = attr + "='";
        start = line.find(pattern);
        if (start == std::string::npos) return "";
    }
    start += pattern.length();
    char quoteChar = line[start - 1];
    size_t end = line.find(quoteChar, start);
    if (end == std::string::npos) return "";
    return line.substr(start, end - start);
}

float OsmParser::ParseHeight(const std::string& heightStr, const std::string& levelsStr) {
    if (!heightStr.empty()) {
        try {
            std::string clean = heightStr;
            size_t unitPos = clean.find("m");
            if (unitPos != std::string::npos) clean = clean.substr(0, unitPos);
            float h = std::stof(clean);
            if (h > 1.0f && h < 500.0f) return h;
        } catch (...) {}
    }

    if (!levelsStr.empty()) {
        try {
            int levels = std::stoi(levelsStr);
            if (levels > 0 && levels < 150) return static_cast<float>(levels) * 3.2f;
        } catch (...) {}
    }

    return 9.0f;
}

void OsmParser::SetOrigin(double lat, double lon) {
    origin_lat_ = lat;
    origin_lon_ = lon;
    has_origin_ = true;
}

Vector2D OsmParser::ProjectLatLon(double lat, double lon) const {
    double dLat = (lat - origin_lat_) * DEG2RAD;
    double dLon = (lon - origin_lon_) * DEG2RAD;
    double meanLat = origin_lat_ * DEG2RAD;

    float x = static_cast<float>(dLon * EARTH_RADIUS * std::cos(meanLat));
    float y = static_cast<float>(dLat * EARTH_RADIUS);
    return {x, y};
}

void OsmParser::AnalyzeBuildingTextures(BuildingData& bldg) {
    uint32_t hash = static_cast<uint32_t>(std::abs(bldg.id));

    // 1. Анализ на Фасадата
    if (bldg.material == "glass" || bldg.levels >= 10 || bldg.buildingType == "office" || bldg.buildingType == "skyscraper") {
        uint16_t glassVariants[] = {8, 9, 10};
        bldg.wallTextureId = glassVariants[hash % 3];
    } else if (bldg.material == "brick" || bldg.colour == "red" || bldg.colour == "brown") {
        bldg.wallTextureId = (hash % 2 == 0) ? 0 : 1;
    } else if (bldg.material == "wood") {
        bldg.wallTextureId = 6;
    } else if (bldg.material == "stone" || bldg.buildingType == "historic" || bldg.buildingType == "church") {
        bldg.wallTextureId = 7;
    } else if (bldg.material == "metal" || bldg.buildingType == "industrial" || bldg.buildingType == "warehouse") {
        bldg.wallTextureId = 13;
    } else if (!bldg.shopType.empty() || bldg.buildingType == "retail" || bldg.buildingType == "commercial") {
        bldg.wallTextureId = 14;
    } else if (bldg.colour == "pink") {
        bldg.wallTextureId = 5;
    } else if (bldg.colour == "blue") {
        bldg.wallTextureId = 4;
    } else if (bldg.colour == "yellow") {
        bldg.wallTextureId = 2;
    } else if (bldg.levels >= 4 && bldg.levels < 10) {
        uint16_t blockVariants[] = {12, 2, 3, 11};
        bldg.wallTextureId = blockVariants[hash % 4];
    } else if (bldg.buildingType == "house" || bldg.buildingType == "detached") {
        uint16_t houseVariants[] = {0, 1, 2, 3, 5, 6};
        bldg.wallTextureId = houseVariants[hash % 6];
    } else {
        uint16_t defaultVariants[] = {2, 3, 4, 5, 11, 12, 15};
        bldg.wallTextureId = defaultVariants[hash % 7];
    }

    // 2. Анализ на Покрива
    if (bldg.roofMaterial == "tiles" || bldg.roofColour == "red") {
        bldg.roofTextureId = 16;
    } else if (bldg.roofMaterial == "slate" || bldg.roofColour == "black" || bldg.roofColour == "dark") {
        bldg.roofTextureId = 17;
    } else if (bldg.roofMaterial == "metal" || bldg.roofMaterial == "copper") {
        bldg.roofTextureId = 19;
    } else if (bldg.levels >= 6) {
        bldg.roofTextureId = (hash % 3 == 0) ? 21 : 18;
    } else if (bldg.buildingType == "house" || bldg.buildingType == "detached") {
        bldg.roofTextureId = (hash % 2 == 0) ? 16 : 17;
    } else {
        uint16_t roofVariants[] = {16, 17, 18, 20};
        bldg.roofTextureId = roofVariants[hash % 4];
    }
}

bool OsmParser::ParseStream(const std::string& filePath, std::function<void(float)> progressCb) {
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t totalBytes = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string line;
    bool inWay = false;
    int64_t currentWayId = 0;
    std::vector<int64_t> currentWayRefs;
    std::string buildingTag = "", highwayTag = "", naturalTag = "", leisureTag = "", landuseTag = "";
    std::string heightTag = "", levelsTag = "", materialTag = "", colourTag = "", roofMatTag = "", roofColTag = "", shopTag = "";

    size_t processedBytes = 0;
    size_t lastReport = 0;

    while (std::getline(file, line)) {
        processedBytes += line.length() + 1;

        if (processedBytes - lastReport > 200000) {
            lastReport = processedBytes;
            if (progressCb && totalBytes > 0) {
                progressCb(static_cast<float>(processedBytes) / static_cast<float>(totalBytes) * 0.5f);
            }
        }

        if (line.find("<node") != std::string::npos) {
            std::string idStr = ExtractAttr(line, "id");
            std::string latStr = ExtractAttr(line, "lat");
            std::string lonStr = ExtractAttr(line, "lon");

            if (!idStr.empty() && !latStr.empty() && !lonStr.empty()) {
                try {
                    int64_t id = std::stoll(idStr);
                    double lat = std::stod(latStr);
                    double lon = std::stod(lonStr);

                    if (!has_origin_) SetOrigin(lat, lon);
                    node_lookup_[id] = ProjectLatLon(lat, lon);
                } catch (...) {}
            }
            continue;
        }

        if (line.find("<way") != std::string::npos) {
            inWay = true;
            std::string idStr = ExtractAttr(line, "id");
            currentWayId = idStr.empty() ? 0 : std::stoll(idStr);
            currentWayRefs.clear();
            buildingTag.clear(); highwayTag.clear(); naturalTag.clear(); leisureTag.clear(); landuseTag.clear();
            heightTag.clear(); levelsTag.clear(); materialTag.clear(); colourTag.clear(); roofMatTag.clear(); roofColTag.clear(); shopTag.clear();
            continue;
        }

        if (inWay) {
            if (line.find("<nd") != std::string::npos) {
                std::string refStr = ExtractAttr(line, "ref");
                if (!refStr.empty()) {
                    try { currentWayRefs.push_back(std::stoll(refStr)); } catch (...) {}
                }
            } else if (line.find("<tag") != std::string::npos) {
                std::string k = ExtractAttr(line, "k");
                std::string v = ExtractAttr(line, "v");

                if (k == "building") buildingTag = v;
                else if (k == "highway") highwayTag = v;
                else if (k == "natural") naturalTag = v;
                else if (k == "leisure") leisureTag = v;
                else if (k == "landuse") landuseTag = v;
                else if (k == "height") heightTag = v;
                else if (k == "building:levels") levelsTag = v;
                else if (k == "building:material") materialTag = v;
                else if (k == "building:colour" || k == "colour") colourTag = v;
                else if (k == "roof:material") roofMatTag = v;
                else if (k == "roof:colour") roofColTag = v;
                else if (k == "shop" || k == "amenity") shopTag = v;
            } else if (line.find("</way>") != std::string::npos) {
                inWay = false;

                if (!buildingTag.empty() && currentWayRefs.size() >= 3) {
                    BuildingData bldg;
                    bldg.id = currentWayId;
                    bldg.buildingType = buildingTag;
                    bldg.material = materialTag;
                    bldg.colour = colourTag;
                    bldg.roofMaterial = roofMatTag;
                    bldg.roofColour = roofColTag;
                    bldg.shopType = shopTag;
                    bldg.height = ParseHeight(heightTag, levelsTag);
                    bldg.levels = levelsTag.empty() ? 3 : std::max(1, std::atoi(levelsTag.c_str()));

                    for (int64_t ref : currentWayRefs) {
                        auto it = node_lookup_.find(ref);
                        if (it != node_lookup_.end()) bldg.footprint.push_back(it->second);
                    }

                    if (bldg.footprint.size() >= 3) {
                        AnalyzeBuildingTextures(bldg);
                        parsed_buildings_.push_back(std::move(bldg));
                        total_buildings_++;
                    }
                } else if (!highwayTag.empty() && currentWayRefs.size() >= 2) {
                    RoadSegment road;
                    road.id = currentWayId;
                    road.highwayType = highwayTag;
                    road.width = (highwayTag == "primary" || highwayTag == "motorway") ? 10.0f : ((highwayTag == "secondary") ? 8.0f : 5.5f);
                    road.roadTextureId = (highwayTag == "primary" || highwayTag == "motorway") ? 22 : ((highwayTag == "pedestrian") ? 24 : 23);

                    for (int64_t ref : currentWayRefs) {
                        auto it = node_lookup_.find(ref);
                        if (it != node_lookup_.end()) road.points.push_back(it->second);
                    }

                    if (road.points.size() >= 2) parsed_roads_.push_back(std::move(road));
                } else if ((naturalTag == "water" || naturalTag == "sand" || leisureTag == "park" || landuseTag == "grass") && currentWayRefs.size() >= 3) {
                    TerrainPolygon terrain;
                    terrain.id = currentWayId;
                    terrain.terrainType = (naturalTag == "water") ? "water" : ((naturalTag == "sand") ? "sand" : "grass");
                    terrain.terrainTextureId = (naturalTag == "water") ? 30 : ((naturalTag == "sand") ? 28 : 26);

                    for (int64_t ref : currentWayRefs) {
                        auto it = node_lookup_.find(ref);
                        if (it != node_lookup_.end()) terrain.points.push_back(it->second);
                    }

                    if (terrain.points.size() >= 3) parsed_terrain_.push_back(std::move(terrain));
                }
            }
        }
    }

    file.close();
    PartitionIntoChunks(250.0f);
    node_lookup_.clear();
    return !chunks_.empty();
}

void OsmParser::PartitionIntoChunks(float chunkSizeMeters) {
    std::map<std::pair<int, int>, MapChunk> chunkMap;

    for (const auto& bldg : parsed_buildings_) {
        if (bldg.footprint.empty()) continue;
        float cx = 0.0f, cy = 0.0f;
        for (const auto& pt : bldg.footprint) { cx += pt.x; cy += pt.y; }
        cx /= static_cast<float>(bldg.footprint.size());
        cy /= static_cast<float>(bldg.footprint.size());

        int cellX = static_cast<int>(std::floor(cx / chunkSizeMeters));
        int cellY = static_cast<int>(std::floor(cy / chunkSizeMeters));

        auto& targetChunk = chunkMap[{cellX, cellY}];
        targetChunk.chunkX = cellX;
        targetChunk.chunkY = cellY;
        targetChunk.chunkName = "chunk_" + std::to_string(cellX) + "_" + std::to_string(cellY);
        targetChunk.buildings.push_back(bldg);
    }

    for (const auto& road : parsed_roads_) {
        if (road.points.empty()) continue;
        int cellX = static_cast<int>(std::floor(road.points[0].x / chunkSizeMeters));
        int cellY = static_cast<int>(std::floor(road.points[0].y / chunkSizeMeters));

        auto& targetChunk = chunkMap[{cellX, cellY}];
        targetChunk.chunkX = cellX;
        targetChunk.chunkY = cellY;
        targetChunk.chunkName = "chunk_" + std::to_string(cellX) + "_" + std::to_string(cellY);
        targetChunk.roads.push_back(road);
    }

    for (const auto& terr : parsed_terrain_) {
        if (terr.points.empty()) continue;
        int cellX = static_cast<int>(std::floor(terr.points[0].x / chunkSizeMeters));
        int cellY = static_cast<int>(std::floor(terr.points[0].y / chunkSizeMeters));

        auto& targetChunk = chunkMap[{cellX, cellY}];
        targetChunk.chunkX = cellX;
        targetChunk.chunkY = cellY;
        targetChunk.chunkName = "chunk_" + std::to_string(cellX) + "_" + std::to_string(cellY);
        targetChunk.terrain.push_back(terr);
    }

    chunks_.reserve(chunkMap.size());
    for (auto& pair : chunkMap) chunks_.push_back(std::move(pair.second));
}
