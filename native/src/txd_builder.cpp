#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cmath>

static constexpr uint32_t rwID_STRUCT       = 0x0001;
static constexpr uint32_t rwID_EXTENSION    = 0x0003;
static constexpr uint32_t rwID_TEXDICTIONARY= 0x0016;
static constexpr uint32_t rwID_TEXNATIVE    = 0x0015;
static constexpr uint32_t RW_VERSION_GTAVC  = 0x0C02FFFF;

static const char* kTextureNames[32] = {
    "fac_brick_red", "fac_brick_white", "fac_plaster_yellow", "fac_plaster_beige",
    "fac_plaster_blue", "fac_plaster_pink", "fac_wood_panel", "fac_stone_rustic",
    "fac_glass_blue", "fac_glass_dark", "fac_glass_green", "fac_concrete_modern",
    "fac_concrete_panel", "fac_metal_cladding", "fac_storefront_shop", "fac_artdeco_white",
    "roof_tile_red", "roof_tile_dark", "roof_tar_gravel", "roof_metal_sheet",
    "roof_concrete", "roof_solar", "road_asphalt_dark", "road_asphalt_city",
    "road_cobblestone", "ground_sidewalk", "ground_grass_lush", "ground_grass_dry",
    "ground_sand_beach", "ground_dirt_gravel", "water_ocean_blue", "water_river_cyan"
};

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

static uint32_t RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(g) << 8)  | static_cast<uint32_t>(r);
}

static void GenerateTexturePixels(const std::string& name, int width, int height, std::vector<uint32_t>& outPixels) {
    outPixels.resize(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t color = RGBA(255, 255, 255);

            // 1. ФАСАДИ
            if (name == "fac_brick_red") {
                bool mortar = (y % 8 == 0) || ((y / 8) % 2 == 0 && x % 16 == 0) || ((y / 8) % 2 != 0 && (x + 8) % 16 == 0);
                color = mortar ? RGBA(180, 175, 170) : RGBA(155 + (x%5)*4, 55 + (y%5)*3, 40);
            } else if (name == "fac_brick_white") {
                bool mortar = (y % 8 == 0) || ((y / 8) % 2 == 0 && x % 16 == 0) || ((y / 8) % 2 != 0 && (x + 8) % 16 == 0);
                color = mortar ? RGBA(110, 110, 110) : RGBA(215 + (x%5)*3, 215 + (y%5)*3, 220);
            } else if (name == "fac_plaster_yellow") {
                uint8_t noise = (x * 7 + y * 13) % 15;
                color = RGBA(225 - noise, 195 - noise, 100 - noise);
            } else if (name == "fac_plaster_beige") {
                uint8_t noise = (x * 11 + y * 17) % 12;
                color = RGBA(220 - noise, 210 - noise, 190 - noise);
            } else if (name == "fac_plaster_blue") {
                uint8_t noise = (x * 5 + y * 7) % 15;
                color = RGBA(110 - noise, 175 - noise, 215 - noise);
            } else if (name == "fac_plaster_pink") {
                uint8_t noise = (x * 9 + y * 11) % 15;
                color = RGBA(240 - noise, 130 - noise, 170 - noise);
            } else if (name == "fac_wood_panel") {
                bool groove = (y % 8 == 0);
                uint8_t woodGrain = (x * 3 + y * 19) % 20;
                color = groove ? RGBA(80, 45, 20) : RGBA(160 + woodGrain, 105 + woodGrain, 60);
            } else if (name == "fac_stone_rustic") {
                bool mortar = (y % 12 == 0) || (x % 16 == 0);
                uint8_t stoneNoise = ((x^y) * 17) % 30;
                color = mortar ? RGBA(70, 70, 70) : RGBA(140 + stoneNoise, 135 + stoneNoise, 130 + stoneNoise);
            } else if (name == "fac_glass_blue") {
                bool mullion = (x % 16 == 0) || (y % 16 == 0);
                uint8_t reflect = ((x + y) * 2) % 40;
                color = mullion ? RGBA(40, 50, 60) : RGBA(30 + reflect, 110 + reflect, 190 + reflect);
            } else if (name == "fac_glass_dark") {
                bool mullion = (x % 16 == 0) || (y % 16 == 0);
                uint8_t reflect = ((x * 2 + y) * 3) % 25;
                color = mullion ? RGBA(20, 20, 25) : RGBA(40 + reflect, 45 + reflect, 50 + reflect);
            } else if (name == "fac_glass_green") {
                bool mullion = (x % 16 == 0) || (y % 16 == 0);
                uint8_t reflect = ((x + y * 2)) % 35;
                color = mullion ? RGBA(30, 45, 35) : RGBA(25 + reflect, 140 + reflect, 100 + reflect);
            } else if (name == "fac_concrete_modern") {
                uint8_t concreteNoise = ((x * 13 + y * 23) % 20);
                bool seam = (x == 0 || y == 0);
                color = seam ? RGBA(110, 110, 110) : RGBA(160 + concreteNoise, 160 + concreteNoise, 165 + concreteNoise);
            } else if (name == "fac_concrete_panel") {
                bool window = (x % 16 >= 4 && x % 16 <= 12) && (y % 16 >= 4 && y % 16 <= 12);
                color = window ? RGBA(50, 80, 110) : RGBA(170, 170, 170);
            } else if (name == "fac_metal_cladding") {
                bool rib = (x % 4 == 0);
                color = rib ? RGBA(120, 125, 130) : RGBA(190, 195, 200);
            } else if (name == "fac_storefront_shop") {
                bool frame = (x < 2 || x > width - 3 || y < 2 || y > height - 3 || y == 20);
                color = frame ? RGBA(180, 40, 40) : (y < 20 ? RGBA(230, 210, 170) : RGBA(40, 70, 95));
            } else if (name == "fac_artdeco_white") {
                bool trim = (y % 16 < 3) || (x % 32 == 0);
                color = trim ? RGBA(0, 190, 210) : RGBA(245, 245, 240);
            }

            // 2. ПОКРИВИ
            else if (name == "roof_tile_red") {
                bool overlap = (y % 8 == 0) || ((y / 8) % 2 == 0 && x % 8 == 0);
                color = overlap ? RGBA(130, 45, 25) : RGBA(195, 80, 45);
            } else if (name == "roof_tile_dark") {
                bool overlap = (y % 8 == 0) || ((y / 8) % 2 == 0 && x % 8 == 0);
                color = overlap ? RGBA(30, 30, 35) : RGBA(65, 68, 75);
            } else if (name == "roof_tar_gravel") {
                uint8_t gravel = ((x * 37 + y * 43) % 40);
                color = RGBA(55 + gravel, 55 + gravel, 58 + gravel);
            } else if (name == "roof_metal_sheet") {
                bool seam = (x % 8 == 0);
                color = seam ? RGBA(100, 120, 110) : RGBA(145, 165, 155);
            } else if (name == "roof_concrete") {
                uint8_t noise = (x * 17 + y * 29) % 25;
                color = RGBA(130 + noise, 130 + noise, 132 + noise);
            } else if (name == "roof_solar") {
                bool grid = (x % 8 == 0) || (y % 12 == 0);
                color = grid ? RGBA(190, 190, 190) : RGBA(15, 30, 75);
            }

            // 3. ПЪТИЩА, ТРОТОАРИ И ПРИРОДА
            else if (name == "road_asphalt_dark") {
                bool yellowLine = (x >= width / 2 - 1 && x <= width / 2 + 1) && (y % 16 < 10);
                color = yellowLine ? RGBA(235, 195, 30) : RGBA(35, 35, 38);
            } else if (name == "road_asphalt_city") {
                bool whiteLine = (x >= width / 2 - 1 && x <= width / 2 + 1) && (y % 16 < 10);
                color = whiteLine ? RGBA(230, 230, 230) : RGBA(50, 50, 52);
            } else if (name == "road_cobblestone") {
                bool joint = (x % 8 == 0) || (y % 8 == 0);
                uint8_t stone = ((x^y) * 19) % 30;
                color = joint ? RGBA(40, 38, 35) : RGBA(115 + stone, 110 + stone, 105 + stone);
            } else if (name == "ground_sidewalk") {
                bool slab = (x % 16 == 0) || (y % 16 == 0);
                color = slab ? RGBA(110, 110, 110) : RGBA(175, 175, 175);
            } else if (name == "ground_grass_lush") {
                uint8_t g = 110 + ((x * 7 + y * 13) % 45);
                color = RGBA(35, g, 25);
            } else if (name == "ground_grass_dry") {
                uint8_t g = 135 + ((x * 5 + y * 11) % 35);
                color = RGBA(140, g, 40);
            } else if (name == "ground_sand_beach") {
                uint8_t sand = ((x * 23 + y * 31) % 25);
                color = RGBA(225 - sand, 195 - sand, 130 - sand);
            } else if (name == "ground_dirt_gravel") {
                uint8_t dirt = ((x * 19 + y * 27) % 35);
                color = RGBA(120 + dirt, 85 + dirt, 50 + dirt);
            } else if (name == "water_ocean_blue") {
                uint8_t wave = ((x * 3 + y * 5) % 40);
                color = RGBA(20, 75 + wave, 185 + wave);
            } else if (name == "water_river_cyan") {
                uint8_t wave = ((x * 4 + y * 6) % 35);
                color = RGBA(30, 160 + wave, 195 + wave);
            }

            outPixels[y * width + x] = color;
        }
    }
}

static TxdWriter BuildNativeTexture(const std::string& texName, int width, int height, int platform) {
    TxdWriter native;
    TxdWriter nativeStruct;

    uint32_t platformId = (platform == 1) ? 8 : 9;
    nativeStruct.Write<uint32_t>(platformId);
    nativeStruct.Write<uint16_t>(0x1106);
    nativeStruct.Write<uint16_t>(0);

    char nameBuf[32];
    char maskBuf[32];
    std::memset(nameBuf, 0, 32);
    std::memset(maskBuf, 0, 32);
    std::strncpy(nameBuf, texName.c_str(), 31);
    nativeStruct.WriteBytes(nameBuf, 32);
    nativeStruct.WriteBytes(maskBuf, 32);

    nativeStruct.Write<uint32_t>(0x1500);
    nativeStruct.Write<uint32_t>(0);
    nativeStruct.Write<uint16_t>(static_cast<uint16_t>(width));
    nativeStruct.Write<uint16_t>(static_cast<uint16_t>(height));
    nativeStruct.Write<uint8_t>(32);
    nativeStruct.Write<uint8_t>(1);
    nativeStruct.Write<uint8_t>(4);
    nativeStruct.Write<uint8_t>(0);

    uint32_t dataSize = width * height * 4;
    nativeStruct.Write<uint32_t>(dataSize);

    std::vector<uint32_t> pixels;
    GenerateTexturePixels(texName, width, height, pixels);
    nativeStruct.WriteBytes(pixels.data(), dataSize);

    native.EmbedChunk(rwID_STRUCT, nativeStruct);
    native.WriteEmptyExtension();
    return native;
}

bool ExportSharedTxd(const std::string& outPath, int platform) {
    TxdWriter txd;

    TxdWriter txdStruct;
    txdStruct.Write<uint16_t>(32); // 32 Текстури
    txdStruct.Write<uint16_t>(0);
    txd.EmbedChunk(rwID_STRUCT, txdStruct);

    for (int i = 0; i < 32; ++i) {
        txd.EmbedChunk(rwID_TEXNATIVE, BuildNativeTexture(kTextureNames[i], 64, 64, platform));
    }

    txd.WriteEmptyExtension();

    std::ofstream out(outPath, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;

    TxdWriter file;
    file.EmbedChunk(rwID_TEXDICTIONARY, txd);
    out.write(reinterpret_cast<const char*>(file.buffer.data()), file.buffer.size());
    out.close();

    return true;
}
