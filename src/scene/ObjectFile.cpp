#include "scene/ObjectFile.h"

#include <cstring>

namespace tmx {

namespace {

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

float ReadF32(const uint8_t* p) {
    uint32_t u = ReadU32(p);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

bool InRange(uint32_t v, uint32_t lo, uint32_t hi) { return v >= lo && v <= hi; }

} // namespace

ObjectKind ClassifyObjectType(uint32_t t) {
    if (t == 2)  return ObjectKind::Sea;
    if (t == 3 || t == 5) return ObjectKind::Float;
    if (t == 4 || t == 6 || t == 7 || t == 343) return ObjectKind::Butterfly;
    if (t == 12 || t == 344) return ObjectKind::Fish;
    if (InRange(t, 311, 322)) return ObjectKind::Leaf;
    if (InRange(t, 331, 342) || InRange(t, 351, 378)) return ObjectKind::Tree;
    if (InRange(t, 487, 489)) return ObjectKind::Ship;
    if (InRange(t, 501, 505)) return ObjectKind::TorchEffect;

    // TMHouse list (TMObjectContainer.cpp:342-368)
    static const uint32_t kHouse[] = {
        251, 252, 253, 254, 474, 273, 274, 292, 607, 610, 614, 195, 697, 699,
        490, 1520, 1535, 1526, 1665, 1993, 2005, 1695, 1696, 1750, 1739, 1711, 1855,
    };
    for (uint32_t h : kHouse) {
        if (t == h)
            return ObjectKind::House;
    }

    return ObjectKind::GenericStatic;
}

bool ObjectFile::Load(const uint8_t* data, size_t size, std::string* err) {
    constexpr size_t kRecord = 28;

    size_t count = size / kRecord;
    records.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const uint8_t* p = data + i * kRecord;
        ObjectFileRecord r;
        r.dwObjType        = ReadU32(p + 0);
        r.posX             = ReadF32(p + 4);
        r.posY             = ReadF32(p + 8);
        r.fHeight          = ReadF32(p + 12);
        r.fAngle           = ReadF32(p + 16);
        r.nTextureSetIndex = (int32_t)ReadU32(p + 20);
        r.nMaskIndex       = (int32_t)ReadU32(p + 24);
        records.push_back(r);

        ObjectKind k = ClassifyObjectType(r.dwObjType);
        skipped[(int)k] += (k == ObjectKind::GenericStatic) ? 0 : 1;
    }

    if (size % kRecord != 0 && err)
        *err = "trailing bytes after last record";
    return true;
}

}
