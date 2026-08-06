#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tmx {

// One record of env\*.dat. On disk each record is 28 bytes — SMALLER than the
// original's ObjectFileItem struct (36 bytes, Structures.h:416), which the original
// reads via struct cast but advances only 28 bytes per record
// (TMObjectContainer.cpp:85-100). We read the real 28-byte layout field-by-field;
// fScaleH/fScaleV are not part of the record (the original's reads of them leak
// the next record's header — a quirk irrelevant to static-mesh rendering).
struct ObjectFileRecord {
    uint32_t dwObjType;
    float    posX, posY;   // TMVector2 (y here is world Z)
    float    fHeight;
    float    fAngle;
    int32_t  nTextureSetIndex;
    int32_t  nMaskIndex;
};

// dwObjType routing, mirroring TMObjectContainer::Load's special cases.
// Phase 1 renders only GenericStatic — everything else is skipped and counted.
enum class ObjectKind {
    GenericStatic,   // plain TMObject -> GetCommonMesh(dwObjType)
    Sea,             // 2
    Float,           // 3, 5
    Butterfly,       // 4, 6, 7, 343
    Fish,            // 12, 344
    Leaf,            // 311-322
    Tree,            // 331-342, 351-378 (TMTree: wind animation — phase 2+)
    Ship,            // 487-489
    House,           // 251-254 + explicit list (TMHouse: doors/state — phase 2+)
    TorchEffect,     // 501-505 (billboard effects, no mesh)
};

ObjectKind ClassifyObjectType(uint32_t dwObjType);

struct ObjectFile {
    std::vector<ObjectFileRecord> records;
    int skipped[16] = {};   // count per ObjectKind (index = (int)kind), for the load log

    bool Load(const uint8_t* data, size_t size, std::string* err);
};

}
