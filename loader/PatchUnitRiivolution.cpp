// PatchUnitRiivolution.cpp
//   Written by mkwcat

#include "PatchUnitRiivolution.hpp"
#include "XMLProcessor.hpp"
#include <Log.hpp>
#include <XML/rapidxml.hpp>

PatchUnitRiivolution::PatchUnitRiivolution(
    DiskID diskId, const char* xml, s32 size
)
  : PatchUnit(sizeof(*this), Type::RIIVOLUTION, diskId)
{
    if (size < 0) {
        size = std::strlen(xml) + 1;
    }

    u8* data = ExpandData(size + 1);
    std::memcpy(data, xml, size);
    data[size] = '\0';

    Init();
}

PatchUnitRiivolution::PatchUnitRiivolution(DiskID diskId, IOS::File& file)
  : PatchUnit(sizeof(*this), Type::RIIVOLUTION, diskId)
{
    s32 size = file.GetSize();

    u8* data = ExpandData(size + 1);
    s32 ret = file.Read(data, size);
    if (ret != size) {
        PRINT(Patcher, ERROR, "Failed to read Riivolution XML file: %d", ret);
        return;
    }
    data[size] = '\0';

    Init();
}

void PatchUnitRiivolution::Init()
{
    m_type = Type::RIIVOLUTION;

    // Parse the XML
    XMLProcessor* processor = XMLProcessor::Get(this);
    if (!processor->IsValid()) {
        PRINT(Patcher, ERROR, "Failed to parse Riivolution XML");
        return;
    }

    GetData()[GetDataSize() - 1] = '\0';

    m_valid = true;
}

static bool ProcessBool(const char* value)
{
    assert(value != nullptr);

    return not std::strcmp(value, "true") or not std::strcmp(value, "yes");
}

static u32 ProcessInt(u32 defaultv, const char* value)
{
    assert(value != nullptr);

    u32 result = 0;

    if (value[0] == '0' and value[1] == 'x') {
        // Process hex
        for (u32 i = 2; value[i] != '\0'; i++) {
            if (value[i] >= '0' and value[i] <= '9') {
                result = (result << 4) | (value[i] - '0');
            } else if (value[i] >= 'A' and value[i] <= 'F') {
                result = (result << 4) | (value[i] - 'A' + 10);
            } else if (value[i] >= 'a' and value[i] <= 'f') {
                result = (result << 4) | (value[i] - 'a' + 10);
            } else {
                result = defaultv;
                break;
            }
        }
    } else {
        // Process decimal
        for (u32 i = 0; value[i] != '\0'; i++) {
            if (value[i] >= '0' and value[i] <= '9') {
                result = (result * 10) + (value[i] - '0');
            } else {
                result = defaultv;
                break;
            }
        }
    }

    return result;
}

bool PatchUnitRiivolution::HandlePatch(
    const char* patchId, std::function<bool(const PatchNode&)> callback
)
{
    XMLProcessor* processor = XMLProcessor::Get(this);
    if (!processor->IsValid()) {
        PRINT(Patcher, ERROR, "Failed to parse Riivolution XML");
        return false;
    }

    auto& doc = processor->GetDocument();
    auto* root = doc.first_node();
    assert(root != nullptr);

    for (auto* node = root->first_node(); node != nullptr;
         node = node->next_sibling()) {
        if (std::strcmp(node->name(), "patch") != 0) {
            continue;
        }

        const char* id = node->first_attribute("id")->value();
        if (std::strcmp(id, patchId) != 0) {
            continue;
        }

        for (auto* child = node->first_node(); child != nullptr;
             child = child->next_sibling()) {
            const char* name = child->name();

            if (std::strcmp(name, "file") == 0) {
                FileNode fileNode;

                if (auto* resize = child->first_attribute("resize")) {
                    fileNode.resize = ProcessBool(resize->value());
                }

                if (auto* create = child->first_attribute("create")) {
                    fileNode.create = ProcessBool(create->value());
                }

                if (auto* disc = child->first_attribute("disc")) {
                    fileNode.disc = disc->value();
                }

                if (auto* offset = child->first_attribute("offset")) {
                    fileNode.offset =
                        ProcessInt(fileNode.offset, offset->value());
                }

                if (auto* external = child->first_attribute("external")) {
                    fileNode.external = external->value();
                }

                if (auto* fileoffset = child->first_attribute("fileoffset")) {
                    fileNode.fileoffset =
                        ProcessInt(fileNode.fileoffset, fileoffset->value());
                }

                if (auto* length = child->first_attribute("length")) {
                    fileNode.length =
                        ProcessInt(fileNode.length, length->value());
                }

                if (!callback(fileNode)) {
                    return false;
                }
            } else if (std::strcmp(name, "folder") == 0) {
                FolderNode folderNode;

                if (auto* create = child->first_attribute("create")) {
                    folderNode.create = ProcessBool(create->value());
                }

                if (auto* resize = child->first_attribute("resize")) {
                    folderNode.resize = ProcessBool(resize->value());
                }

                if (auto* recursive = child->first_attribute("recursive")) {
                    folderNode.recursive = ProcessBool(recursive->value());
                }

                if (auto* length = child->first_attribute("length")) {
                    folderNode.length =
                        ProcessInt(folderNode.length, length->value());
                }

                if (auto* disc = child->first_attribute("disc")) {
                    folderNode.disc = disc->value();
                }

                if (auto* external = child->first_attribute("external")) {
                    folderNode.external = external->value();
                }

                if (!callback(folderNode)) {
                    return false;
                }
            } else if (std::strcmp(name, "shift") == 0) {
                ShiftNode shiftNode;

                if (auto* source = child->first_attribute("source")) {
                    shiftNode.source = source->value();
                }

                if (auto* destination = child->first_attribute("destination")) {
                    shiftNode.destination = destination->value();
                }

                if (!callback(shiftNode)) {
                    return false;
                }
            } else if (std::strcmp(name, "savegame") == 0) {
                SavegameNode savegameNode;

                if (auto* external = child->first_attribute("external")) {
                    savegameNode.external = external->value();
                }

                if (auto* clone = child->first_attribute("clone")) {
                    savegameNode.clone = ProcessBool(clone->value());
                }

                if (!callback(savegameNode)) {
                    return false;
                }
            } else if (std::strcmp(name, "dlc") == 0) {
                DLCNode dlcNode;

                if (auto* external = child->first_attribute("external")) {
                    dlcNode.external = external->value();
                }

                if (!callback(dlcNode)) {
                    return false;
                }
            } else if (std::strcmp(name, "memory") == 0) {
                MemoryNode memoryNode;

                if (auto* offset = child->first_attribute("offset")) {
                    memoryNode.offset =
                        ProcessInt(memoryNode.offset, offset->value());
                }

                if (auto* search = child->first_attribute("search")) {
                    memoryNode.search = ProcessBool(search->value());
                }

                if (auto* ocarina = child->first_attribute("ocarina")) {
                    memoryNode.ocarina = ProcessBool(ocarina->value());
                }

                if (auto* align = child->first_attribute("align")) {
                    memoryNode.align =
                        ProcessInt(memoryNode.align, align->value());
                }

                if (auto* valuefile = child->first_attribute("valuefile")) {
                    memoryNode.valuefile = valuefile->value();
                }

                if (auto* value = child->first_attribute("value")) {
                    memoryNode.value = value->value();
                }

                if (auto* original = child->first_attribute("original")) {
                    memoryNode.original = original->value();
                }

                if (!callback(memoryNode)) {
                    return false;
                }
            } else {
                PRINT(Patcher, WARN, "Unknown patch node: %s", name);
            }
        }

        return true;
    }

    PRINT(Patcher, ERROR, "Failed to find patch ID '%s'", patchId);

    return false;
}