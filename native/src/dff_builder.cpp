#include "geometry_builder.h"
#include <fstream>
#include <vector>
#include <cstring>

static constexpr uint32_t rwID_STRUCT       = 0x0001;
static constexpr uint32_t rwID_STRING       = 0x0002;
static constexpr uint32_t rwID_EXTENSION    = 0x0003;
static constexpr uint32_t rwID_TEXTURE      = 0x0006;
static constexpr uint32_t rwID_MATERIAL     = 0x0007;
static constexpr uint32_t rwID_MATLIST      = 0x0008;
static constexpr uint32_t rwID_FRAMELIST    = 0x000E;
static constexpr uint32_t rwID_GEOMETRY     = 0x000F;
static constexpr uint32_t rwID_CLUMP        = 0x0010;
static constexpr uint32_t rwID_ATOMIC       = 0x0014;
static constexpr uint32_t rwID_GEOMETRYLIST = 0x001A;
static constexpr uint32_t RW_VERSION_GTAVC  = 0x0C02FFFF;

class BinaryStream {
public:
    std::vector<uint8_t> buffer;

    template<typename T>
    void Write(T val) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&val);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
    }

    void WriteBytes(const void* data, size_t size) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
        buffer.insert(buffer.end(), ptr, ptr + size);
    }

    void WriteChunkHeader(uint32_t type, uint32_t length) {
        Write<uint32_t>(type);
        Write<uint32_t>(length);
        Write<uint32_t>(RW_VERSION_GTAVC);
    }

    void EmbedChunk(uint32_t type, const BinaryStream& subStream) {
        WriteChunkHeader(type, static_cast<uint32_t>(subStream.buffer.size()));
        WriteBytes(subStream.buffer.data(), subStream.buffer.size());
    }

    void WriteEmptyExtension() {
        WriteChunkHeader(rwID_EXTENSION, 0);
    }

    void WriteStringChunk(const std::string& str) {
        BinaryStream s;
        s.WriteBytes(str.c_str(), str.length() + 1);
        while (s.buffer.size() % 4 != 0) {
            s.Write<uint8_t>(0);
        }
        EmbedChunk(rwID_STRING, s);
    }
};

static BinaryStream BuildMaterial(const std::string& texName) {
    BinaryStream mat;
    BinaryStream matStruct;
    matStruct.Write<uint32_t>(0);
    matStruct.Write<uint32_t>(0xFFFFFFFF);
    matStruct.Write<uint32_t>(1);
    matStruct.Write<int32_t>(1);
    matStruct.Write<float>(1.0f);
    matStruct.Write<float>(1.0f);
    matStruct.Write<float>(1.0f);
    mat.EmbedChunk(rwID_STRUCT, matStruct);

    BinaryStream tex;
    BinaryStream texStruct;
    texStruct.Write<uint16_t>(0x1106);
    texStruct.Write<uint16_t>(0);
    tex.EmbedChunk(rwID_STRUCT, texStruct);
    tex.WriteStringChunk(texName);
    tex.WriteStringChunk("");
    tex.WriteEmptyExtension();
    mat.EmbedChunk(rwID_TEXTURE, tex);

    mat.WriteEmptyExtension();
    return mat;
}

bool ExportChunkDff(const MapChunk& chunk, const std::string& outPath, int platform) {
    (void)platform;
    ChunkMesh mesh = GeometryBuilder::BuildMesh(chunk);
    if (mesh.vertices.empty()) return true;

    BinaryStream clump;

    BinaryStream clumpStruct;
    clumpStruct.Write<uint32_t>(1);
    clumpStruct.Write<uint32_t>(0);
    clumpStruct.Write<uint32_t>(0);
    clump.EmbedChunk(rwID_STRUCT, clumpStruct);

    BinaryStream frameList;
    BinaryStream frameStruct;
    frameStruct.Write<uint32_t>(1);
    float matrix[9] = {1,0,0, 0,1,0, 0,0,1};
    float pos[3] = {0,0,0};
    frameStruct.WriteBytes(matrix, sizeof(matrix));
    frameStruct.WriteBytes(pos, sizeof(pos));
    frameStruct.Write<int32_t>(-1);
    frameStruct.Write<uint32_t>(0);
    frameList.EmbedChunk(rwID_STRUCT, frameStruct);
    frameList.WriteEmptyExtension();
    clump.EmbedChunk(rwID_FRAMELIST, frameList);

    BinaryStream geomList;
    BinaryStream geomListStruct;
    geomListStruct.Write<uint32_t>(1);
    geomList.EmbedChunk(rwID_STRUCT, geomListStruct);

    BinaryStream geom;
    BinaryStream geomStruct;
    uint16_t flags = 0x0001 | 0x0004 | 0x0010 | 0x0008;
    geomStruct.Write<uint16_t>(flags);
    geomStruct.Write<uint16_t>(0);
    geomStruct.Write<uint32_t>(static_cast<uint32_t>(mesh.triangles.size()));
    geomStruct.Write<uint32_t>(static_cast<uint32_t>(mesh.vertices.size()));
    geomStruct.Write<uint32_t>(1);

    for (const auto& v : mesh.vertices) geomStruct.Write<uint32_t>(v.color);
    for (const auto& v : mesh.vertices) {
        geomStruct.Write<float>(v.u);
        geomStruct.Write<float>(v.v);
    }
    for (const auto& t : mesh.triangles) {
        geomStruct.Write<uint16_t>(t.b);
        geomStruct.Write<uint16_t>(t.a);
        geomStruct.Write<uint16_t>(t.materialId);
        geomStruct.Write<uint16_t>(t.c);
    }

    geomStruct.Write<float>(mesh.bounds.center.x);
    geomStruct.Write<float>(mesh.bounds.center.y);
    geomStruct.Write<float>(mesh.bounds.center.z);
    geomStruct.Write<float>(mesh.bounds.radius);
    geomStruct.Write<uint32_t>(1);
    geomStruct.Write<uint32_t>(1);

    for (const auto& v : mesh.vertices) {
        geomStruct.Write<float>(v.x);
        geomStruct.Write<float>(v.y);
        geomStruct.Write<float>(v.z);
    }
    for (const auto& v : mesh.vertices) {
        geomStruct.Write<float>(v.nx);
        geomStruct.Write<float>(v.ny);
        geomStruct.Write<float>(v.nz);
    }

    geom.EmbedChunk(rwID_STRUCT, geomStruct);

    // 5 Материала: wall, roof, road, grass, water
    BinaryStream matList;
    BinaryStream matListStruct;
    matListStruct.Write<uint32_t>(5);
    for (int i = 0; i < 5; ++i) matListStruct.Write<int32_t>(-1);
    matList.EmbedChunk(rwID_STRUCT, matListStruct);

    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_wall"));
    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_roof"));
    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_road"));
    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_grass"));
    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_water"));

    geom.EmbedChunk(rwID_MATLIST, matList);
    geom.WriteEmptyExtension();

    geomList.EmbedChunk(rwID_GEOMETRY, geom);
    clump.EmbedChunk(rwID_GEOMETRYLIST, geomList);

    BinaryStream atomic;
    BinaryStream atomicStruct;
    atomicStruct.Write<uint32_t>(0);
    atomicStruct.Write<uint32_t>(0);
    atomicStruct.Write<uint32_t>(5);
    atomicStruct.Write<uint32_t>(0);
    atomic.EmbedChunk(rwID_STRUCT, atomicStruct);
    atomic.WriteEmptyExtension();
    clump.EmbedChunk(rwID_ATOMIC, atomic);

    clump.WriteEmptyExtension();

    std::ofstream out(outPath, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;

    BinaryStream finalFile;
    finalFile.EmbedChunk(rwID_CLUMP, clump);
    out.write(reinterpret_cast<const char*>(finalFile.buffer.data()), finalFile.buffer.size());
    out.close();

    return true;
}
