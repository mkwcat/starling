#pragma once

class PatchUnit;

namespace rapidxml
{
template<class Ch>
class xml_document;
}

class XMLProcessor
{
public:
    static XMLProcessor* Get(PatchUnit* patchUnit);

    XMLProcessor();

    XMLProcessor(PatchUnit* patchUnit, const char* xml);

    ~XMLProcessor();

    // XML processing functions

    bool IsValid() const
    {
        return m_valid;
    }

    rapidxml::xml_document<char>& GetDocument()
    {
        return m_doc;
    }

private:
    PatchUnit* m_patchUnit;
    rapidxml::xml_document<char>& m_doc;
    bool m_valid;
};