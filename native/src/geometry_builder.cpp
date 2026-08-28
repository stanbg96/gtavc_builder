#include "geometry_builder.h"
#include <cmath>
#include <algorithm>

static constexpr float UV_WALL_SCALE = 0.25f;
static constexpr float UV_ROOF_SCALE = 0.15f;
static constexpr float UV_TERRAIN_SCALE = 0.10f;

static float CrossProduct2D(const Vector2D& a, const Vector2D& b, const Vector2D& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool PointInTriangle(const Vector2D& pt, const Vector2D& a, const Vector2D& b, const Vector2D& c) {
    float cp1 = CrossProduct2D(a, b, pt);
    float cp2 = CrossProduct2D(b, c, pt);
    float cp3 = CrossProduct2D(c, a, pt);
    return ((cp1 >= 0 && cp2 >= 0 && cp3 >= 0) || (cp1 <= 0 && cp2 <= 0 && cp3 <= 0));
}

bool GeometryBuilder::IsEar(const std::vector<Vector2D>& p, int u, int v, int w, int n, const int* V) {
    Vector2D A = p[V[u]];
    Vector2D B = p[V[v]];
    Vector2D C = p[V[w]];

    if (CrossProduct2D(A, B, C) <= 0.000001f) return false;

    for (int pIdx = 0; pIdx < n; pIdx++) {
        if (pIdx == u || pIdx == v || pIdx == w) continue;
        Vector2D P = p[V[pIdx]];
        if (PointInTriangle(P, A, B, C)) return false;
    }
    return true;
}

bool GeometryBuilder::TriangulatePolygon(const std::vector<Vector2D>& polygon, std::vector<int>& outIndices) {
    int n = static_cast<int>(polygon.size());
    if (n < 3) return false;

    std::vector<int> V(n);
    float area = 0.0f;
    for (int p = n - 1, q = 0; q < n; p = q++) {
        area += polygon[p].x * polygon[q].y - polygon[q].x * polygon[p].y;
    }

    if (area > 0.0f) {
        for (int v = 0; v < n; v++) V[v] = v;
    } else {
        for (int v = 0; v < n; v++) V[v] = (n - 1) - v;
    }

    int count = 2 * n;
    for (int v = n - 1; n > 2;) {
        if (count-- <= 0) return false;

        int u = v;
        if (n <= u) u = 0;
        v = u + 1;
        if (n <= v) v = 0;
        int w = v + 1;
        if (n <= w) w = 0;

        if (IsEar(polygon, u, v, w, n, V.data())) {
            outIndices.push_back(V[u]);
            outIndices.push_back(V[v]);
            outIndices.push_back(V[w]);

            for (int s = v, t = v + 1; t < n; s++, t++) {
                V[s] = V[t];
            }
            n--;
            count = 2 * n;
        }
    }
    return true;
}

void GeometryBuilder::ExtrudeBuilding(const BuildingData& bldg, ChunkMesh& mesh) {
    const auto& poly = bldg.footprint;
    size_t pts = poly.size();
    if (pts < 3) return;

    float totalDist = 0.0f;
    for (size_t i = 0; i < pts; ++i) {
        size_t next = (i + 1) % pts;
        const Vector2D& p1 = poly[i];
        const Vector2D& p2 = poly[next];

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float segLen = std::sqrt(dx * dx + dy * dy);
        if (segLen < 0.001f) continue;

        float nx = -dy / segLen;
        float ny = dx / segLen;

        uint16_t baseIdx = static_cast<uint16_t>(mesh.vertices.size());

        float u1 = totalDist * UV_WALL_SCALE;
        float u2 = (totalDist + segLen) * UV_WALL_SCALE;
        float vBottom = 0.0f;
        float vTop = bldg.height * UV_WALL_SCALE;

        Vertex3D v0 = {p1.x, p1.y, 0.0f, nx, ny, 0.0f, 0xFFFFFFFF, u1, vBottom};
        Vertex3D v1 = {p2.x, p2.y, 0.0f, nx, ny, 0.0f, 0xFFFFFFFF, u2, vBottom};
        Vertex3D v2 = {p2.x, p2.y, bldg.height, nx, ny, 0.0f, 0xFFFFFFFF, u2, vTop};
        Vertex3D v3 = {p1.x, p1.y, bldg.height, nx, ny, 0.0f, 0xFFFFFFFF, u1, vTop};

        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3);

        mesh.triangles.push_back({baseIdx, static_cast<uint16_t>(baseIdx + 1), static_cast<uint16_t>(baseIdx + 2), 0});
        mesh.triangles.push_back({baseIdx, static_cast<uint16_t>(baseIdx + 2), static_cast<uint16_t>(baseIdx + 3), 0});

        totalDist += segLen;
    }

    TriangulateRoof(poly, bldg.height, mesh);
}

void GeometryBuilder::TriangulateRoof(const std::vector<Vector2D>& poly, float height, ChunkMesh& mesh) {
    std::vector<int> indices;
    if (!TriangulatePolygon(poly, indices)) return;

    uint16_t baseIdx = static_cast<uint16_t>(mesh.vertices.size());

    for (const auto& pt : poly) {
        Vertex3D v;
        v.x = pt.x;
        v.y = pt.y;
        v.z = height;
        v.nx = 0.0f;
        v.ny = 0.0f;
        v.nz = 1.0f;
        v.color = 0xFFCCCCCC;
        v.u = pt.x * UV_ROOF_SCALE;
        v.v = pt.y * UV_ROOF_SCALE;
        mesh.vertices.push_back(v);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        TriangleFace tri;
        tri.a = static_cast<uint16_t>(baseIdx + indices[i]);
        tri.b = static_cast<uint16_t>(baseIdx + indices[i + 1]);
        tri.c = static_cast<uint16_t>(baseIdx + indices[i + 2]);
        tri.materialId = 1; // Roof
        mesh.triangles.push_back(tri);
    }
}

void GeometryBuilder::GenerateRoad(const RoadSegment& road, ChunkMesh& mesh) {
    const auto& pts = road.points;
    if (pts.size() < 2) return;

    float halfWidth = road.width * 0.5f;
    float currentU = 0.0f;

    for (size_t i = 0; i < pts.size() - 1; ++i) {
        const Vector2D& p1 = pts[i];
        const Vector2D& p2 = pts[i + 1];

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) continue;

        float nx = -dy / len * halfWidth;
        float ny = dx / len * halfWidth;

        uint16_t baseIdx = static_cast<uint16_t>(mesh.vertices.size());
        float nextU = currentU + len * 0.1f;

        Vertex3D v0 = {p1.x - nx, p1.y - ny, 0.05f, 0.0f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, currentU};
        Vertex3D v1 = {p1.x + nx, p1.y + ny, 0.05f, 0.0f, 0.0f, 1.0f, 0xFFFFFFFF, 1.0f, currentU};
        Vertex3D v2 = {p2.x + nx, p2.y + ny, 0.05f, 0.0f, 0.0f, 1.0f, 0xFFFFFFFF, 1.0f, nextU};
        Vertex3D v3 = {p2.x - nx, p2.y - ny, 0.05f, 0.0f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, nextU};

        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3);

        mesh.triangles.push_back({baseIdx, static_cast<uint16_t>(baseIdx + 1), static_cast<uint16_t>(baseIdx + 2), 2});
        mesh.triangles.push_back({baseIdx, static_cast<uint16_t>(baseIdx + 2), static_cast<uint16_t>(baseIdx + 3), 2});

        currentU = nextU;
    }
}

void GeometryBuilder::GenerateTerrain(const TerrainPolygon& terr, ChunkMesh& mesh) {
    std::vector<int> indices;
    if (!TriangulatePolygon(terr.points, indices)) return;

    uint16_t baseIdx = static_cast<uint16_t>(mesh.vertices.size());
    float zHeight = (terr.terrainType == "water") ? -0.3f : 0.01f;
    uint16_t matId = (terr.terrainType == "water") ? 4 : 3; // 4 = Water, 3 = Grass

    for (const auto& pt : terr.points) {
        Vertex3D v;
        v.x = pt.x;
        v.y = pt.y;
        v.z = zHeight;
        v.nx = 0.0f;
        v.ny = 0.0f;
        v.nz = 1.0f;
        v.color = 0xFFFFFFFF;
        v.u = pt.x * UV_TERRAIN_SCALE;
        v.v = pt.y * UV_TERRAIN_SCALE;
        mesh.vertices.push_back(v);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        TriangleFace tri;
        tri.a = static_cast<uint16_t>(baseIdx + indices[i]);
        tri.b = static_cast<uint16_t>(baseIdx + indices[i + 1]);
        tri.c = static_cast<uint16_t>(baseIdx + indices[i + 2]);
        tri.materialId = matId;
        mesh.triangles.push_back(tri);
    }
}

void GeometryBuilder::ComputeBounds(ChunkMesh& mesh) {
    if (mesh.vertices.empty()) return;

    mesh.bounds.min = {mesh.vertices[0].x, mesh.vertices[0].y, mesh.vertices[0].z};
    mesh.bounds.max = mesh.bounds.min;

    for (const auto& v : mesh.vertices) {
        mesh.bounds.min.x = std::min(mesh.bounds.min.x, v.x);
        mesh.bounds.min.y = std::min(mesh.bounds.min.y, v.y);
        mesh.bounds.min.z = std::min(mesh.bounds.min.z, v.z);

        mesh.bounds.max.x = std::max(mesh.bounds.max.x, v.x);
        mesh.bounds.max.y = std::max(mesh.bounds.max.y, v.y);
        mesh.bounds.max.z = std::max(mesh.bounds.max.z, v.z);
    }

    mesh.bounds.center.x = (mesh.bounds.min.x + mesh.bounds.max.x) * 0.5f;
    mesh.bounds.center.y = (mesh.bounds.min.y + mesh.bounds.max.y) * 0.5f;
    mesh.bounds.center.z = (mesh.bounds.min.z + mesh.bounds.max.z) * 0.5f;

    float maxDistSq = 0.0f;
    for (const auto& v : mesh.vertices) {
        float dx = v.x - mesh.bounds.center.x;
        float dy = v.y - mesh.bounds.center.y;
        float dz = v.z - mesh.bounds.center.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > maxDistSq) maxDistSq = distSq;
    }
    mesh.bounds.radius = std::sqrt(maxDistSq);
}

ChunkMesh GeometryBuilder::BuildMesh(const MapChunk& chunk) {
    ChunkMesh mesh;
    for (const auto& bldg : chunk.buildings) {
        ExtrudeBuilding(bldg, mesh);
    }
    for (const auto& road : chunk.roads) {
        GenerateRoad(road, mesh);
    }
    for (const auto& terr : chunk.terrain) {
        GenerateTerrain(terr, mesh);
    }
    ComputeBounds(mesh);
    return mesh;
}
