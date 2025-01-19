#include "PatchManager.hpp"
#include "PatchUnit.hpp"
#include "PatchUnitRiivolution.hpp"
#include <AddressMap.h>
#include <IOS.hpp>
#include <Log.hpp>
#include <XML/rapidxml.hpp>
#include <cassert>

PatchUnit* PatchManager::s_first;
PatchUnit* PatchManager::s_last;

void PatchManager::StaticInit()
{
    s_first = reinterpret_cast<PatchUnit*>(PATCH_LIST_ADDRESS);
    s_first->m_next = nullptr;
    s_first->m_type = PatchUnit::Type::DISABLED;
    s_last = s_first;
}

bool PatchManager::LoadRiivolutionXML(const char* path)
{
    // TODO: Handle directory path

    PRINT(Patcher, INFO, "Loading Riivolution XML file '%s'", path);

    IOS::File xmlFile(path, IOS::Mode::READ);
    if (!xmlFile.IsValid()) {
        PRINT(
            Patcher, ERROR, "Failed to open Riivolution XML file: %d",
            xmlFile.GetFd()
        );
        return false;
    }

    PatchUnitRiivolution* patchUnit =
        CreatePatchUnit<PatchUnitRiivolution>(0, xmlFile);
    assert(patchUnit != nullptr);

    return patchUnit->IsValid();
}

bool PatchManager::LoadPatchID(const char* patchId)
{
    PRINT(Patcher, INFO, "Loading patch ID '%s'", patchId);

    for (PatchUnit* patchUnit = s_first; patchUnit != nullptr;
         patchUnit = patchUnit->m_next) {
        PatchUnitRiivolution* riivolution =
            PatchUnitRiivolution::Get(patchUnit);
        if (riivolution == nullptr) {
            continue;
        }

        if (!riivolution->HandlePatch(
                patchId,
                [riivolution](const PatchUnitRiivolution::PatchNode& node) {
            return HandlePatchNode(riivolution, node);
        }
            )) {
            return false;
        }

        return true;
    }

    PRINT(Patcher, ERROR, "Failed to find patch ID '%s'", patchId);
    return false;
}

bool PatchManager::HandlePatchNode(
    [[maybe_unused]] PatchUnitRiivolution* unit,
    const PatchUnitRiivolution::PatchNode& node
)
{
    if (auto* fileNode = std::get_if<PatchUnitRiivolution::FileNode>(&node)) {
        // File node

        if (fileNode->disc == nullptr) {
            PRINT(Patcher, ERROR, "File node missing 'disc' attribute");
            return false;
        }

        PRINT(Patcher, INFO, "File node: %s", fileNode->disc);
    }

    return true;
}