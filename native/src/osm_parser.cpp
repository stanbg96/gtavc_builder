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
            if (unitPos != std::string::npos) {
                clean = clean.substr(0, unitPos);
            }
            float h = std::stof(clean);
            if (h > 1.0f && h < 500.0f) return h;
        } catch (...) {}
    }

    if (!levelsStr.empty()) {
        try {
            int levels = std::stoi(levelsStr);
            if (levels > 0 && levels < 150) {
                return static_cast<float>(levels) * 3.2f;
            }
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
    std::string buildingTagValue = "";
    std::string highwayTagValue = "";
    std::string naturalTagValue = "";
    std::string leisureTagValue = "";
    std::string landuseTagValue = "";
    std::string heightTagValue = "";
    std::string levelsTagValue = "";

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

                    if (!has_origin_) {
                        SetOrigin(lat, lon);
                    }

                    Vector2D localPos = ProjectLatLon(lat, lon);
                    node_lookup_[id] = localPos;
                } catch (...) {}
            }
            continue;
        }

        if (line.find("<way") != std::string::npos) {
            inWay = true;
            std::string idStr = ExtractAttr(line, "id");
            currentWayId = idStr.empty() ? 0 : std::stoll(idStr);
            currentWayRefs.clear();
            buildingTagValue.clear();
            highwayTagValue.clear();
            naturalTagValue.clear();
            leisureTagValue.clear();
            landuseTagValue.clear();
            heightTagValue.clear();
            levelsTagValue.clear();
            continue;
        }

        if (inWay) {
            if (line.find("<nd") != std::string::npos) {
                std::string refStr = ExtractAttr(line, "ref");
                if (!refStr.empty()) {
                    try {
                        currentWayRefs.push_back(std::stoll(refStr));
                    } catch (...) {}
                }
            } else if (line.find("<tag") != std::string::npos) {
                std::string k = ExtractAttr(line, "k");
                std::string v = ExtractAttr(line, "v");

                if (k == "building") buildingTagValue = v;
                else if (k == "highway") highwayTagValue = v;
                else if (k == "natural") naturalTagValue = v;
                else if (k == "leisure") leisureTagValue = v;
                else if (k == "landuse") landuseTagValue = v;
                else if (k == "height") heightTagValue = v;
                else if (k == "building:levels") levelsTagValue = v;
            } else if (line.find("</way>") != std::string::npos) {
                inWay = false;

                // 1. Сгради
                if (!buildingTagValue.empty() && currentWayRefs.size() >= 3) {
                    BuildingData bldg;
                    bldg.id = currentWayId;
                    bldg.buildingType = buildingTagValue;
                    bldg.height = ParseHeight(heightTagValue, levelsTagValue);
                    bldg.levels = levelsTagValue.empty() ? 3 : std::max(1, std::atoi(levelsTagValue.c_str()));

                    for (int64_t ref : currentWayRefs) {
                        auto it = node_lookup_.find(ref);
                        if (it != node_lookup_.end()) bldg.footprint.push_back(it->second);
                    }

                    if (bldg.footprint.size() >= 3) {
                        parsed_buildings_.push_back(std::move(bldg));
                        total_buildings_++;
                    }
                }
                // 2. Пътна мрежа
                else if (!highwayTagValue.empty() && currentWayRefs.size() >= 2) {
                    RoadSegment road;
                    road.id = currentWayId;
                    road.highwayType = highwayTagValue;
                    road.width = (highwayTagValue == "primary" || highwayTagValue == "secondary") ? 9.0f : 5.5f;

                    for (int64_t ref : currentWayRefs) {
                        auto it = node_lookup_.find(ref);
                        if (it != node_lookup_.end()) road.points.push_back(it->second);
                    }

                    if (road.points.size() >= 2) {
                        parsed_roads_.push_back(std::move(road));
                    }
                }
                // 3. Водни площи и Паркове
                else if ((naturalTagValue == "water" || leisureTagValue == "park" || landuseTagValue == "grass") && currentWayRefs.size() >= 3) {
                    TerrainPolygon terrain;
                    terrain.id = currentWayId;
                    terrain.terrainType = (naturalTagValue == "water") ? "water" : "grass";

                    for (int64_t ref : currentWayRefs) {
                        auto it = node_lookup_.find(ref);
                        if (it != node_lookup_.end()) terrain.points.push_back(it->second);
                    }

                    if (terrain.points.size() >= 3) {
                        parsed_terrain_.push_back(std::move(terrain));
                    }
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
    for (auto& pair : chunkMap) {
        chunks_.push_back(std::move(pair.second));
    }
}
