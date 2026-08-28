#include <iostream>
#include <string>
#include <vector>
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

// Декларации на строителите от Стъпка 4
bool ExportChunkDff(const MapChunk& chunk, const std::string& outPath, int platform);
bool ExportChunkCol(const MapChunk& chunk, const std::string& outPath);
bool ExportSharedTxd(const std::string& outPath, int platform);

extern "C" {

#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
int ProcessOsmData(const char* osmPath, const char* outDir, int targetPlatform, void (*progressCb)(float)) {
    if (!osmPath || !outDir) {
        LOGE("Невалидни входни параметри за ProcessOsmData");
        return -1;
    }

    std::string osmFilePath(osmPath);
    std::string outputDirectory(outDir);

    LOGI("Започва обработка на OSM файл: %s", osmFilePath.c_str());
    if (progressCb) progressCb(0.05f);

    OsmParser parser;
    bool parseSuccess = parser.ParseStream(osmFilePath, [&](float p) {
        if (progressCb) progressCb(p);
    });

    if (!parseSuccess) {
        LOGE("Грешка при парсване на OSM XML файла!");
        return -2;
    }

    const auto& chunks = parser.GetChunks();
    LOGI("Успешно генерирани %zu порции (chunks) с общо %zu сгради", chunks.size(), parser.GetTotalBuildings());

    // 1. Генериране на общ TXD (Texture Dictionary) файл за района
    std::string txdPath = outputDirectory + "/osm_world.txd";
    if (!ExportSharedTxd(txdPath, targetPlatform)) {
        LOGE("Грешка при експортиране на TXD файл");
        return -3;
    }

    // 2. Генериране на .DFF (3D модели) и .COL (Колизии) за всеки Chunk
    float startProgress = 0.55f;
    float step = 0.45f / static_cast<float>(std::max<size_t>(1, chunks.size()));

    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        std::string dffPath = outputDirectory + "/" + chunk.chunkName + ".dff";
        std::string colPath = outputDirectory + "/" + chunk.chunkName + ".col";

        if (!ExportChunkDff(chunk, dffPath, targetPlatform)) {
            LOGE("Грешка при запис на DFF: %s", dffPath.c_str());
        }

        if (!ExportChunkCol(chunk, colPath)) {
            LOGE("Грешка при запис на COL: %s", colPath.c_str());
        }

        if (progressCb) {
            progressCb(startProgress + static_cast<float>(i + 1) * step);
        }
    }

    LOGI("Генерирането на GTA Vice City бинарните файлове завърши успешно!");
    if (progressCb) progressCb(1.0f);

    return 0;
}

}
