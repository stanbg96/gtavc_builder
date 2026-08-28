#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "osm_parser.h"

#if defined(__ANDROID__)
#include <android/log.h>
#define LOG_TAG "GTAVC_ENGINE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

bool ExportChunkDff(const MapChunk& chunk, const std::string& outPath, int platform);
bool ExportChunkCol(const MapChunk& chunk, const std::string& outPath);
bool ExportSharedTxd(const std::string& outPath, int platform);

static constexpr size_t SECTOR_SIZE = 2048;

#pragma pack(push, 1)
struct DirEntry {
    uint32_t offset;
    uint32_t size;
    char name[24];
};
#pragma pack(pop)

struct PropInstance {
    int id;
    std::string modelName;
    Vector3D pos;
    float rotZ;
};

struct ParkedCar {
    Vector3D pos;
    float angle;
    int modelId;
};

static bool IsPointInPoly(const Vector2D& pt, const std::vector<Vector2D>& poly) {
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > pt.y) != (poly[j].y > pt.y)) &&
            (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

static std::vector<PropInstance> GenerateWorldProps(const std::vector<MapChunk>& chunks) {
    std::vector<PropInstance> props;

    for (const auto& chunk : chunks) {
        for (const auto& road : chunk.roads) {
            float halfWidth = road.width * 0.5f + 1.2f;
            for (size_t i = 0; i < road.points.size() - 1; ++i) {
                const auto& p1 = road.points[i];
                const auto& p2 = road.points[i + 1];

                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len < 15.0f) continue;

                float nx = -dy / len;
                float ny = dx / len;
                float rotZ = std::atan2(dy, dx);

                for (float d = 10.0f; d < len - 5.0f; d += 35.0f) {
                    float t = d / len;
                    float px = p1.x + t * dx;
                    float py = p1.y + t * dy;

                    PropInstance lamp;
                    lamp.id = 622;
                    lamp.modelName = "lamppost1";
                    lamp.pos = {px + nx * halfWidth, py + ny * halfWidth, 0.0f};
                    lamp.rotZ = rotZ;
                    props.push_back(lamp);
                }
            }
        }

        for (const auto& terr : chunk.terrain) {
            if (terr.terrainType != "grass" || terr.points.size() < 3) continue;

            float minX = terr.points[0].x, maxX = terr.points[0].x;
            float minY = terr.points[0].y, maxY = terr.points[0].y;
            for (const auto& pt : terr.points) {
                minX = std::min(minX, pt.x); maxX = std::max(maxX, pt.x);
                minY = std::min(minY, pt.y); maxY = std::max(maxY, pt.y);
            }

            for (float x = minX + 8.0f; x < maxX - 8.0f; x += 22.0f) {
                for (float y = minY + 8.0f; y < maxY - 8.0f; y += 22.0f) {
                    if (IsPointInPoly({x, y}, terr.points)) {
                        PropInstance tree;
                        tree.id = 650;
                        tree.modelName = "veg_palmb01";
                        tree.pos = {x, y, 0.0f};
                        tree.rotZ = static_cast<float>((static_cast<int>(x + y) % 360)) * 0.01745f;
                        props.push_back(tree);
                    }
                }
            }
        }
    }
    return props;
}

static std::vector<ParkedCar> GenerateParkedCars(const std::vector<MapChunk>& chunks, int primaryVehicleId) {
    std::vector<ParkedCar> cars;

    ParkedCar startCar;
    startCar.pos = {0.0f, 0.0f, 0.3f};
    startCar.angle = 90.0f;
    startCar.modelId = primaryVehicleId;
    cars.push_back(startCar);

    int euroJapPool[] = {141, 236, 188, 205, 138, 139, 135, 196, 208, 189, 130, 191, 198, 168};
    const size_t poolSize = sizeof(euroJapPool) / sizeof(euroJapPool[0]);
    size_t poolIdx = 0;

    for (const auto& chunk : chunks) {
        for (const auto& road : chunk.roads) {
            if (road.points.size() < 2) continue;
            for (size_t i = 0; i < road.points.size() - 1; ++i) {
                const auto& p1 = road.points[i];
                const auto& p2 = road.points[i + 1];
                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 35.0f) {
                    float nx = -dy / len * (road.width * 0.5f - 1.2f);
                    float ny = dx / len * (road.width * 0.5f - 1.2f);

                    ParkedCar car;
                    car.pos = {p1.x + dx * 0.5f + nx, p1.y + dy * 0.5f + ny, 0.3f};
                    car.angle = std::atan2(dy, dx) * 57.29577f;
                    car.modelId = euroJapPool[(poolIdx++) % poolSize];
                    cars.push_back(car);
                }
            }
        }
    }
    return cars;
}

static bool ExportIplFile(const std::vector<MapChunk>& chunks, const std::vector<PropInstance>& props, const std::vector<ParkedCar>& cars, const std::string& outPath) {
    std::ofstream out(outPath);
    if (!out.is_open()) return false;

    out << "# Generated by GTA VC Map Builder (OSM)\n";
    out << "inst\n";

    int startId = 18000;
    for (size_t i = 0; i < chunks.size(); ++i) {
        out << (startId + i) << ", " << chunks[i].chunkName << ", 0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0\n";
    }

    for (const auto& prop : props) {
        float qz = std::sin(prop.rotZ * 0.5f);
        float qw = std::cos(prop.rotZ * 0.5f);
        out << prop.id << ", " << prop.modelName << ", 0, "
            << prop.pos.x << ", " << prop.pos.y << ", " << prop.pos.z
            << ", 1.0, 1.0, 1.0, 0.0, 0.0, " << qz << ", " << qw << "\n";
    }

    out << "end\n";
    out << "cull\nend\npath\nend\n";

    out << "cars\n";
    for (const auto& c : cars) {
        out << c.pos.x << ", " << c.pos.y << ", " << c.pos.z << ", "
            << c.angle << ", " << c.modelId << ", -1, -1, 1, 0, 0, 0, 0\n";
    }
    out << "end\n";
    out.close();
    return true;
}

static bool ExportIdeFile(const std::vector<MapChunk>& chunks, const std::string& outPath) {
    std::ofstream out(outPath);
    if (!out.is_open()) return false;

    out << "# Generated by GTA VC Map Builder (OSM)\n";
    out << "objs\n";
    int startId = 18000;
    for (size_t i = 0; i < chunks.size(); ++i) {
        out << (startId + i) << ", " << chunks[i].chunkName << ", osm_world, 1, 650, 0\n";
    }
    out << "end\n";
    out << "txdp\nend\n2dfx\nend\n";
    out.close();
    return true;
}

static bool PackImgArchive(const std::vector<std::string>& filePaths, const std::string& outImgPath, const std::string& outDirPath) {
    std::ofstream imgFile(outImgPath, std::ios::out | std::ios::binary);
    std::ofstream dirFile(outDirPath, std::ios::out | std::ios::binary);
    if (!imgFile.is_open() || !dirFile.is_open()) return false;

    uint32_t currentSector = 0;
    for (const auto& path : filePaths) {
        std::ifstream src(path, std::ios::in | std::ios::binary | std::ios::ate);
        if (!src.is_open()) continue;

        size_t fileSize = src.tellg();
        src.seekg(0, std::ios::beg);
        std::vector<char> buffer(fileSize);
        src.read(buffer.data(), fileSize);
        src.close();

        size_t lastSlash = path.find_last_of("/\\");
        std::string fileName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        uint32_t sectorCount = static_cast<uint32_t>((fileSize + SECTOR_SIZE - 1) / SECTOR_SIZE);

        DirEntry entry;
        std::memset(&entry, 0, sizeof(DirEntry));
        entry.offset = currentSector;
        entry.size = sectorCount;
        std::strncpy(entry.name, fileName.c_str(), 23);

        dirFile.write(reinterpret_cast<const char*>(&entry), sizeof(DirEntry));
        imgFile.write(buffer.data(), fileSize);

        size_t padBytes = (sectorCount * SECTOR_SIZE) - fileSize;
        if (padBytes > 0) {
            std::vector<char> padding(padBytes, 0);
            imgFile.write(padding.data(), padBytes);
        }
        currentSector += sectorCount;
    }
    return true;
}

static bool MergeColFiles(const std::vector<std::string>& colPaths, const std::string& mergedPath) {
    std::ofstream out(mergedPath, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;
    for (const auto& path : colPaths) {
        std::ifstream src(path, std::ios::in | std::ios::binary);
        if (src.is_open()) { out << src.rdbuf(); src.close(); }
    }
    return true;
}

static bool ExportInstallGuide(const std::vector<MapChunk>& chunks, size_t propCount, size_t carCount, int texturingMode, const std::string& outPath) {
    std::ofstream out(outPath);
    if (!out.is_open()) return false;

    out << "=================================================================\n";
    out << "   GTA VICE CITY - ИНСТРУКЦИИ ЗА ИНСТАЛИРАНЕ НА КАРТАТА         \n";
    out << "=================================================================\n\n";
    out << "• Режим на текстуриране: " << (texturingMode == 0 ? "AI Сателитен Анализ (Real Imagery)" : "32 Процедурни Материала") << "\n";
    out << "• 3D Квартали: " << chunks.size() << "\n";
    out << "• Улични лампи и палми: " << propCount << "\n";
    out << "• Паркирани автомобили: " << carCount << "\n\n";
    out << "1. СТЪПКА: Копирайте всички файлове от тази папка в:\n";
    out << "   GTA Vice City/data/maps/osm/\n\n";
    out << "2. СТЪПКА: Добавете следните 4 реда в data/gta_vc.dat:\n\n";
    out << "   CDIMAGE DATA\\MAPS\\OSM\\OSM_MAP.IMG\n";
    out << "   IDE DATA\\MAPS\\OSM\\OSM_WORLD.IDE\n";
    out << "   IPL DATA\\MAPS\\OSM\\OSM_WORLD.IPL\n";
    out << "   COLFILE 0 DATA\\MAPS\\OSM\\OSM_WORLD.COL\n\n";
    out << "3. СТЪПКА: СТАРТИРАЙТЕ ИГРАТА!\n";
    out << "   Координати на новия град: X: 0.0, Y: 0.0, Z: 15.0\n";
    out << "=================================================================\n";
    return true;
}

extern "C" {

#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
int ProcessOsmData(const char* osmPath, const char* outDir, const char* satImagePath, int targetPlatform, int enableProps, int spawnVehicleId, int texturingMode, void (*progressCb)(float)) {
    if (!osmPath || !outDir) return -1;

    std::string osmFilePath(osmPath);
    std::string outputDirectory(outDir);
    std::string satPath(satImagePath ? satImagePath : "");

    LOGI("Започва обработка на OSM: %s (Mode: %d, Sat: %s)", osmFilePath.c_str(), texturingMode, satPath.c_str());
    if (progressCb) progressCb(0.05f);

    OsmParser parser;
    if (!parser.ParseStream(osmFilePath, [&](float p) { if (progressCb) progressCb(p); })) {
        return -2;
    }

    const auto& chunks = parser.GetChunks();
    std::string txdPath = outputDirectory + "/osm_world.txd";
    ExportSharedTxd(txdPath, targetPlatform);

    std::vector<PropInstance> props;
    if (enableProps) {
        props = GenerateWorldProps(chunks);
    }

    std::vector<ParkedCar> cars = GenerateParkedCars(chunks, spawnVehicleId);

    ExportIdeFile(chunks, outputDirectory + "/osm_world.ide");
    ExportIplFile(chunks, props, cars, outputDirectory + "/osm_world.ipl");
    ExportInstallGuide(chunks, props.size(), cars.size(), texturingMode, outputDirectory + "/install_guide.txt");

    std::vector<std::string> dffFiles = {txdPath};
    std::vector<std::string> colFiles;

    float startProgress = 0.55f;
    float step = 0.30f / static_cast<float>(std::max<size_t>(1, chunks.size()));

    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        std::string dffPath = outputDirectory + "/" + chunk.chunkName + ".dff";
        std::string colPath = outputDirectory + "/" + chunk.chunkName + ".col";

        ExportChunkDff(chunk, dffPath, targetPlatform);
        ExportChunkCol(chunk, colPath);

        dffFiles.push_back(dffPath);
        colFiles.push_back(colPath);

        if (progressCb) progressCb(startProgress + static_cast<float>(i + 1) * step);
    }

    PackImgArchive(dffFiles, outputDirectory + "/osm_map.img", outputDirectory + "/osm_map.dir");
    MergeColFiles(colFiles, outputDirectory + "/osm_world.col");

    if (progressCb) progressCb(1.0f);
    return 0;
}

}
