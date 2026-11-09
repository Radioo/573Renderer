#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

struct ScriptParam {
    std::string name;
    std::string type;
    std::string said;
};

struct ScriptDoc {
    std::string name;
    std::string kind;
    std::string signature;
    std::string summary;
    std::string returns;
    std::vector<ScriptParam> params;
    std::string id;
    bool call = false;
    bool measured = false;
};

[[nodiscard]] std::optional<ScriptDoc> ScriptWordDoc(std::string_view word);

}
