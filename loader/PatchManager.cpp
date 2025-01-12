#include "PatchManager.hpp"
#include "PatchUnit.hpp"
#include "PatchUnitRiivolution.hpp"
#include <IOS.hpp>
#include <Log.hpp>
#include <XML/rapidxml.hpp>

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

    s32 size = xmlFile.GetSize();
    if (size == 0) {
        PRINT(Patcher, ERROR, "Riivolution XML file is empty");
        return false;
    }

    char xmlData[size + 1];
    s32 ret = xmlFile.Read(xmlData, size);
    if (ret != size) {
        PRINT(Patcher, ERROR, "Failed to read Riivolution XML file: %d", ret);
        return false;
    }

    xmlData[size] = '\0';

    PRINT(Patcher, INFO, "Riivolution XML file data: %s", xmlData);

    rapidxml::xml_document<> doc;
    doc.parse<0>(xmlData);

    PRINT(
        Patcher, INFO, "Riivolution XML file parsed: %s",
        doc.first_node()->name()
    );

    return true;
}

void rapidxml::parse_error_handler(
    const char* what, [[maybe_unused]] void* where
)
{
    PRINT(Patcher, ERROR, "Riivolution XML parse error: %s", what);
}