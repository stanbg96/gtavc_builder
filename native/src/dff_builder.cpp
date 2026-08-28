#include "geometry_builder.h"
#include <fstream>
#include <vector>
#include <cstring>

// RenderWare 3.4 IDs
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

// RenderWare Version Stamp за GTA Vice City (3.4.0.3)
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
        // Подравняване до 4 байта
        while (s.buffer.size() % 4 != 0) {
            s.Write<uint8_t>(0);
        }
        EmbedChunk(rwID_STRING, s);
    }
};

static BinaryStream BuildMaterial(const std::string& texName) {
    BinaryStream mat;

    // rwID_STRUCT за Материал
    BinaryStream matStruct;
    matStruct.Write<uint32_t>(0);          // flags
    matStruct.Write<uint32_t>(0xFFFFFFFF); // Color RGBA
    matStruct.Write<uint32_t>(1);          // Unused
    matStruct.Write<int32_t>(1);           // isTextured = true
    matStruct.Write<float>(1.0f);          // ambient
    matStruct.Write<float>(1.0f);          // specular
    matStruct.Write<float>(1.0f);          // diffuse
    mat.EmbedChunk(rwID_STRUCT, matStruct);

    // rwID_TEXTURE
    BinaryStream tex;
    BinaryStream texStruct;
    texStruct.Write<uint16_t>(0x1106);     // Filter & addressing (wrap)
    texStruct.Write<uint16_t>(0);          // Padding
    tex.EmbedChunk(rwID_STRUCT, texStruct);
    tex.WriteStringChunk(texName);
    tex.WriteStringChunk("");              // Mask Name
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

    // 1. Clump Struct
    BinaryStream clumpStruct;
    clumpStruct.Write<uint32_t>(1); // numAtomics
    clumpStruct.Write<uint32_t>(0); // numLights
    clumpStruct.Write<uint32_t>(0); // numCameras
    clump.EmbedChunk(rwID_STRUCT, clumpStruct);

    // 2. FrameList
    BinaryStream frameList;
    BinaryStream frameStruct;
    frameStruct.Write<uint32_t>(1); // 1 frame
    // Identity Matrix
    float matrix[9] = {1,0,0, 0,1,0, 0,0,1};
    float pos[3] = {0,0,0};
    frameStruct.WriteBytes(matrix, sizeof(matrix));
    frameStruct.WriteBytes(pos, sizeof(pos));
    frameStruct.Write<int32_t>(-1); // Parent frame index
    frameStruct.Write<uint32_t>(0); // Frame flags
    frameList.EmbedChunk(rwID_STRUCT, frameStruct);
    frameList.WriteEmptyExtension();
    clump.EmbedChunk(rwID_FRAMELIST, frameList);

    // 3. GeometryList
    BinaryStream geomList;
    BinaryStream geomListStruct;
    geomListStruct.Write<uint32_t>(1); // 1 geometry
    geomList.EmbedChunk(rwID_STRUCT, geomListStruct);

    // 3.1. Geometry
    BinaryStream geom;
    BinaryStream geomStruct;

    // Flags: Prelit | Textured | Normals | Positions
    uint16_t flags = 0x0001 | 0x0004 | 0x0010 | 0x0008;
    geomStruct.Write<uint16_t>(flags);
    geomStruct.Write<uint16_t>(0); // numUVs = 1
    geomStruct.Write<uint32_t>(static_cast<uint32_t>(mesh.triangles.size()));
    geomStruct.Write<uint32_t>(static_cast<uint32_t>(mesh.vertices.size()));
    geomStruct.Write<uint32_t>(1); // numMorphTargets

    // Prelit colors
    for (const auto& v : mesh.vertices) {
        geomStruct.Write<uint32_t>(v.color);
    }

    // TexCoords (UV)
    for (const auto& v : mesh.vertices) {
        geomStruct.Write<float>(v.u);
        geomStruct.Write<float>(v.v);
    }

    // Triangles: uint16 vertex2, vertex1, matId, vertex3
    for (const auto& t : mesh.triangles) {
        geomStruct.Write<uint16_t>(t.b);
        geomStruct.Write<uint16_t>(t.a);
        geomStruct.Write<uint16_t>(t.materialId);
        geomStruct.Write<uint16_t>(t.c);
    }

    // Morph Target: Bounding Sphere & Vertices & Normals
    geomStruct.Write<float>(mesh.bounds.center.x);
    geomStruct.Write<float>(mesh.bounds.center.y);
    geomStruct.Write<float>(mesh.bounds.center.z);
    geomStruct.Write<float>(mesh.bounds.radius);
    geomStruct.Write<uint32_t>(1); // Has positions
    geomStruct.Write<uint32_t>(1); // Has normals

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

    // 3.2. MaterialList
    BinaryStream matList;
    BinaryStream matListStruct;
    matListStruct.Write<uint32_t>(3); // 3 Материала: wall, roof, road
    matListStruct.Write<int32_t>(-1);
    matListStruct.Write<int32_t>(-1);
    matListStruct.Write<int32_t>(-1);
    matList.EmbedChunk(rwID_STRUCT, matListStruct);

    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_wall"));
    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_roof"));
    matList.EmbedChunk(rwID_MATERIAL, BuildMaterial("osm_road"));

    geom.EmbedChunk(rwID_MATLIST, matList);
    geom.WriteEmptyExtension();

    geomList.EmbedChunk(rwID_GEOMETRY, geom);
    clump.EmbedChunk(rwID_GEOMETRYLIST, geomList);

    // 4. Atomic
    BinaryStream atomic;
    BinaryStream atomicStruct;
    atomicStruct.Write<uint32_t>(0); // frameIndex
    atomicStruct.Write<uint32_t>(0); // geometryIndex
    atomicStruct.Write<uint32_t>(5); // flags: Render | Collision
    atomicStruct.Write<uint32_t>(0); // unused
    atomic.EmbedChunk(rwID_STRUCT, atomicStruct);
    atomic.WriteEmptyExtension();
    clump.EmbedChunk(rwID_ATOMIC, atomic);

    clump.WriteEmptyExtension();

    // Запис във файла
    std::ofstream out(outPath, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;

    BinaryStream finalFile;
    finalFile.EmbedChunk(rwID_CLUMP, clump);
    out.write(reinterpret_cast<const char*>(finalFile.buffer.data()), finalFile.buffer.size());
    out.close();

    return true;
}
