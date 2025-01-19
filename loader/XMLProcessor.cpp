// XMLProcessor.cpp
//   Written by mkwcat

#include "XMLProcessor.hpp"
#include "PatchUnit.hpp"
#include "PatchUnitRiivolution.hpp"
#include <Log.hpp>
#include <XML/rapidxml.hpp>
#include <csetjmp>
#include <new>

static rapidxml::xml_document s_doc;

XMLProcessor* XMLProcessor::Get(PatchUnit* patchUnit)
{
    static XMLProcessor s_processor;

    if (s_processor.m_patchUnit == patchUnit) {
        return &s_processor;
    }

    s_processor.~XMLProcessor();
    return new (&s_processor)
        XMLProcessor(patchUnit, PatchUnitRiivolution::Get(patchUnit)->GetXML());
}

XMLProcessor::XMLProcessor()
  : m_patchUnit(nullptr)
  , m_doc(s_doc)
  , m_valid(false)
{
}

static std::jmp_buf s_errorHandler;

XMLProcessor::XMLProcessor(PatchUnit* patchUnit, const char* xml)
  : m_patchUnit(patchUnit)
  , m_doc(s_doc)
  , m_valid(false)
{
    if (setjmp(s_errorHandler)) {
        return;
    }

    new (&m_doc) rapidxml::xml_document<char>();

    m_doc.parse<0>(const_cast<char*>(xml));

    if (m_doc.first_node() == nullptr) {
        return;
    }

    m_valid = true;
}

void rapidxml::parse_error_handler(
    const char* what, [[maybe_unused]] void* where
)
{
    PRINT(Patcher, ERROR, "Riivolution XML parse error: %s", what);
    if (where != nullptr) {
        const char* whereStr = static_cast<const char*>(where);
        if (whereStr[0] != '\0') {
            PRINT(Patcher, ERROR, "Note: At: %.16s", whereStr);
        } else {
            PRINT(Patcher, ERROR, "Note: At end of file");
        }
    }
    std::longjmp(s_errorHandler, 1);
}

XMLProcessor::~XMLProcessor()
{
}