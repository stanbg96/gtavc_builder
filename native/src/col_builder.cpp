#include "geometry_builder.h"
#include <fstream>
#include <vector>
#include <cstring>

#pragma pack(push, 1)
struct Col1Header {
    char magic[4];        // "COLL"
    uint32_t size;        // Общ размер след хедъра
    char modelName[22];   // Име на модела
    uint16_t modelId;     // ID
    float radius;         // Bounding Sphere Radius
    Vector3D center;      // Bounding Sphere Center
    Vector3D min;         // Bounding Box Min
    Vector3D max;         // Bounding Box Max
    uint16_t numSpheres;  // 0
    uint16_t numBoxes;    // 0
    uint16_t numFaces;    // Брой полигони за колизия
    uint8_t numLines;     // 0
    uint8_t pad;          // 0
    uint32_t flags;       // 0
    uint32_t numVertices; // Брой върхове
};

struct ColFace {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint8_t surfaceType;  // 1 = Concrete / Asphalt
    uint8_t pieceType;    // 0
    uint8_t brightness;   // 128
    uint8_t light;        // 0
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

    // Изчисляване на размера на Col тялото
    uint32_t bodySize = sizeof(Col1Header) - 8 // без magic и size
                      + header.numVertices * sizeof(Vector3D)
                      + header.numFaces * sizeof(ColFace);
    header.size = bodySize;

    std::ofstream out(outPath, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;

    // 1. Запис на хедъра
    out.write(reinterpret_cast<const char*>(&header), sizeof(Col1Header));

    // 2. Запис на върховете за колизия
    for (const auto& v : mesh.vertices) {
        Vector3D p = {v.x, v.y, v.z};
        out.write(reinterpret_cast<const char*>(&p), sizeof(Vector3D));
    }

    // 3. Запис на повърхностите (Faces)
    for (const auto& t : mesh.triangles) {
        ColFace face;
        face.a = t.a;
        face.b = t.b;
        face.c = t.c;
        face.surfaceType = (t.materialId == 2) ? 2 : 1; // 2: Tarmac/Road, 1: Concrete
        face.pieceType = 0;
        face.brightness = 128;
        face.light = 0;
        out.write(reinterpret_cast<const char*>(&face), sizeof(ColFace));
    }

    out.close();
    return true;
}
