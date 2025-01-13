#ifndef PARSER_XMLPARSER_HPP
#define PARSER_XMLPARSER_HPP

#include <istream>
#include <vector>

#include "parser/IDataParser.hpp"
#include "db/LogEvent.hpp"
#include "db/EventsContainer.hpp"

namespace parser
{

  class XmlParser : public IDataParser
  {
  public:
    XmlParser();
    virtual ~XmlParser() = default;
    void ParseData(const std::string &filepath) override;
    void ParseData(std::istream &input) override;
    uint32_t GetCurrentProgress() const override;
    uint32_t GetTotalProgress() const override;

  private:
    uint32_t m_currentProgress;
    uint32_t m_totalProgress;
  };
} // namespace parser
#endif // PARSER_XMLPARSER_HPP
