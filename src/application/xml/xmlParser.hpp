#ifndef PARSER_XMLPARSER_HPP
#define PARSER_XMLPARSER_HPP

#include <cstddef>
#include <cstdint>
#include <expat.h>

#include <istream>

#include "parser/IDataParser.hpp"

namespace parser
{

class XmlParser : public IDataParser
{
  public:
    XmlParser();
    virtual ~XmlParser() = default;
    // Loads and parses XML file using Expat
    void ParseData(const std::string& filepath) override;
    void ParseData(std::istream& input) override;
    uint32_t GetCurrentProgress() const override;
    uint32_t GetTotalProgress() const override;

  public:
    uint32_t m_currentProgress;
};

struct ParserState
{
    std::string currentElement;
    std::string currentText;
    size_t totalBytes = 0;
    size_t bytesProcessed = 0;
    int eventId = 0;
    bool insideRoot = false;
    bool insideEvent = false;
    db::LogEvent::EventItems eventItems;
    XmlParser* parser = nullptr;
    XML_Parser parserHandle = nullptr; // <-- add this line here
};

} // namespace parser
#endif // PARSER_XMLPARSER_HPP
