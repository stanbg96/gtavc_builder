#pragma once

#include "osm_parser.h"
#include <vector>
#include <cstdint>

struct Vertex3D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 1.0f;
    uint32_t color = 0xFFFFFFFF;
    float u = 0.0f;
    float v = 0.0f;
};

struct TriangleFace {
    uint16_t a = 0;
    uint16_t b = 0;
    uint16_t c = 0;
    uint16_t materialId = 0; // 0: Wall, 1: Roof, 2: Road
};

struct BoundingVolume {
    Vector3D min;
    Vector3D max;
    Vector3D center;
    float radius = 0.0f;
};

struct ChunkMesh {
    std::vector<Vertex3D> vertices;
    std::vector<TriangleFace> triangles;
    BoundingVolume bounds;
};

class GeometryBuilder {
public:
    static ChunkMesh BuildMesh(const MapChunk& chunk);

private:
    static void ExtrudeBuilding(const BuildingData& bldg, ChunkMesh& mesh);
    static void TriangulateRoof(const std::vector<Vector2D>& poly, float height, ChunkMesh& mesh);
    static void GenerateRoad(const RoadSegment& road, ChunkMesh& mesh);
    static void ComputeBounds(ChunkMesh& mesh);
    
    // Ear Clipping за триангулиране на 2D полигони (покриви)
    static bool IsEar(const std::vector<Vector2D>& p, int u, int v, int w, int n, const int* V);
    static bool TriangulatePolygon(const std::vector<Vector2D>& polygon, std::vector<int>& outIndices);
};
