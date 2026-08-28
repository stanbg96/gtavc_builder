#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

static constexpr uint32_t rwID_STRUCT       = 0x0001;
static constexpr uint32_t rwID_EXTENSION    = 0x0003;
static constexpr uint32_t rwID_TEXDICTIONARY= 0x0016;
static constexpr uint32_t rwID_TEXNATIVE    = 0x0015;
static constexpr uint32_t RW_VERSION_GTAVC  = 0x0C02FFFF;

class TxdWriter {
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

    void EmbedChunk(uint32_t type, const TxdWriter& subStream) {
        WriteChunkHeader(type, static_cast<uint32_t>(subStream.buffer.size()));
        WriteBytes(subStream.buffer.data(), subStream.buffer.size());
    }

    void WriteEmptyExtension() {
        WriteChunkHeader(rwID_EXTENSION, 0);
    }
};

static void GenerateTexturePixels(const std::string& name, int width, int height, std::vector<uint32_t>& outPixels) {
    outPixels.resize(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t color = 0xFFFFFFFF;

            if (name == "osm_wall") {
                // Тухлена/фасадна текстура (бежово с линии)
                bool isMortar = (y % 8 == 0) || ((y / 8) % 2 == 0 && x % 16 == 0) || ((y / 8) % 2 != 0 && (x + 8) % 16 == 0);
                color = isMortar ? 0xFF888888 : 0xFF35486B; // BGR: тухлен цвят
            } else if (name == "osm_roof") {
                // Тъмно сив асфалт за покрив с текстурен шум
                uint8_t noise = static_cast<uint8_t>(40 + ((x ^ y) % 25));
                color = (0xFF << 24) | (noise << 16) | (noise << 8) | noise;
            } else if (name == "osm_road") {
                // Асфалт с осева пунктирана линия
                bool isCenterLine = (x >= width / 2 - 1 && x <= width / 2 + 1) && (y % 16 < 10);
                color = isCenterLine ? 0xFFEEEEEE : 0xFF2A2A2A;
            }

            outPixels[y * width + x] = color;
        }
    }
}

static TxdWriter BuildNativeTexture(const std::string& texName, int width, int height, int platform) {
    TxdWriter native;
    TxdWriter nativeStruct;

    uint32_t platformId = (platform == 1) ? 8 : 9; // 9 = PC (Direct3D 8/9), 8 = Mobile/OpenGL
    nativeStruct.Write<uint32_t>(platformId);

    // Filter & addressing
    nativeStruct.Write<uint16_t>(0x1106); // Wrap UV, Linear filter
    nativeStruct.Write<uint16_t>(0);

    // Имена
    char nameBuf[32];
    char maskBuf[32];
    std::memset(nameBuf, 0, 32);
    std::memset(maskBuf, 0, 32);
    std::strncpy(nameBuf, texName.c_str(), 31);
    nativeStruct.WriteBytes(nameBuf, 32);
    nativeStruct.WriteBytes(maskBuf, 32);

    // Raster Format (0x1500: 8888 32-bit BGRA / RGBA)
    nativeStruct.Write<uint32_t>(0x1500); // 32-bit raster
    nativeStruct.Write<uint32_t>(0);      // hasAlpha
    nativeStruct.Write<uint16_t>(static_cast<uint16_t>(width));
    nativeStruct.Write<uint16_t>(static_cast<uint16_t>(height));
    nativeStruct.Write<uint8_t>(32);      // Depth
    nativeStruct.Write<uint8_t>(1);       // Num Mipmaps
    nativeStruct.Write<uint8_t>(4);       // Raster Type
    nativeStruct.Write<uint8_t>(0);       // Compression (None)

    uint32_t dataSize = width * height * 4;
    nativeStruct.Write<uint32_t>(dataSize);

    // Генериране на самите пиксели
    std::vector<uint32_t> pixels;
    GenerateTexturePixels(texName, width, height, pixels);
    nativeStruct.WriteBytes(pixels.data(), dataSize);

    native.EmbedChunk(rwID_STRUCT, nativeStruct);
    native.WriteEmptyExtension();

    return native;
}

bool ExportSharedTxd(const std::string& outPath, int platform) {
    TxdWriter txd;

    // 1. TXD Struct
    TxdWriter txdStruct;
    txdStruct.Write<uint16_t>(3); // 3 текстури в речника
    txdStruct.Write<uint16_t>(0);
    txd.EmbedChunk(rwID_STRUCT, txdStruct);

    // 2. Вграждане на 3-те Native Текстури
    txd.EmbedChunk(rwID_TEXNATIVE, BuildNativeTexture("osm_wall", 64, 64, platform));
    txd.EmbedChunk(rwID_TEXNATIVE, BuildNativeTexture("osm_roof", 64, 64, platform));
    txd.EmbedChunk(rwID_TEXNATIVE, BuildNativeTexture("osm_road", 64, 64, platform));

    txd.WriteEmptyExtension();

    std::ofstream out(outPath, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;

    TxdWriter file;
    file.EmbedChunk(rwID_TEXDICTIONARY, txd);
    out.write(reinterpret_cast<const char*>(file.buffer.data()), file.buffer.size());
    out.close();

    return true;
}
