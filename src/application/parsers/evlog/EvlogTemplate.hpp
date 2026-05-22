#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace parser {

struct EvlogTemplateField {
    enum class Type {
        Int8, Int16, Int32, Int64,
        UInt8, UInt16, UInt32, UInt64,
        Float, Double,
        String,   // fixed-size char buffer (null-terminated within N bytes)
        CString,  // variable-length null-terminated string
    };
    std::string name;
    Type        type      {Type::Int32};
    size_t      fixedSize {0};  // >0 for String[N], 0 for scalar / CString
};

struct EvlogTemplate {
    int32_t  facility  {0};
    uint32_t eventType {0};
    std::string description;
    std::vector<EvlogTemplateField> fields;
    std::string formatStr;  // e.g. "uid=%uid% host=%hostname%"
};

} // namespace parser
