#include "geometry_builder.h"
#include <fstream>
#include <vector>
#include <cstring>

#pragma pack(push, 1)
struct Col1Header {
    char magic[4];
    uint32_t size;
    char modelName[22];
    uint16_t modelId;
    float radius;
    Vector3D center;
    Vector3D min;
    Vector3D max;
    uint16_t numSpheres;
    uint16_t numBoxes;
    uint16_t numFaces;
    uint8_t numLines;
    uint8_t pad;
    uint32_t flags;
    uint32_t numVertices;
};

struct ColFace {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint8_t surfaceType;
    uint8_t pieceType;
    uint8_t brightness;
    uint8_t light;
};
#pragma pack(pop)

bool ExportChunkCol(const MapChunk& chunk, const std::string& outPath) {
    ChunkMesh mesh = GeometryBuilder::BuildMesh(chunk);
    if (mesh.vertices.empty()) return true;

    Col1Header header;
    std::memset(&header, 0, sizeof(Col1Header));
    std::memcpy(header.magic, "COLL", 4);
    std::strncpy(header.modelName, chunk.chunkName.c_str(), 21);
    header.modelId = 1;

    header.radius = mesh.bounds.radius;
    header.center = mesh.bounds.center;
    header.min = mesh.bounds.min;
    header.max = mesh.bounds.max;

    header.numSpheres = 0;
    header.numBoxes = 0;
    header.numLines = 0;
    header.flags = 0;

    header.numVertices = static_cast<uint32_t>(mesh.vertices.size());
    header.numFaces = static_cast<uint16_t>(mesh.triangles.size());

    uint32_t bodySize = sizeof(Col1Header) - 8
                      + header.numVertices * sizeof(Vector3D)
                      + header.numFaces * sizeof(ColFace);
    header.size = bodySize;

    std::ofstream out(outPath, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;

    out.write(reinterpret_cast<const char*>(&header), sizeof(Col1Header));

    for (const auto& v : mesh.vertices) {
        Vector3D p = {v.x, v.y, v.z};
        out.write(reinterpret_cast<const char*>(&p), sizeof(Vector3D));
    }

    for (const auto& t : mesh.triangles) {
        ColFace face;
        face.a = t.a;
        face.b = t.b;
        face.c = t.c;
        // Повърхност: 1: Бетон, 2: Асфалт, 3: Трева, 0: Вода
        if (t.materialId == 2) face.surfaceType = 2;
        else if (t.materialId == 3) face.surfaceType = 3;
        else if (t.materialId == 4) face.surfaceType = 0;
        else face.surfaceType = 1;

        face.pieceType = 0;
        face.brightness = 128;
        face.light = 0;
        out.write(reinterpret_cast<const char*>(&face), sizeof(ColFace));
    }

    out.close();
    return true;
}
