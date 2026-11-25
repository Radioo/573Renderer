#include "judge/verdict_store.h"

#include "judge/afp_pose.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace Judge {

namespace {

constexpr int kFormatVersion = 1;
constexpr int kIndent = 2;

std::optional<AfpPose::MatrixLerp> MatrixLerpFromName(const std::string& name) {
    if (name == AfpPose::MatrixLerpName(AfpPose::MatrixLerp::Decompose))
        return AfpPose::MatrixLerp::Decompose;
    if (name == AfpPose::MatrixLerpName(AfpPose::MatrixLerp::Linear))
        return AfpPose::MatrixLerp::Linear;
    return std::nullopt;
}

bool ReadEntries(const nlohmann::json& doc, std::map<std::string, Entry>& out, std::string& err) {
    if (!doc.is_object() || !doc.contains("verdicts") || !doc["verdicts"].is_object()) {
        err = "it has no \"verdicts\" object";
        return false;
    }
    for (const auto& [key, value] : doc["verdicts"].items()) {
        if (!value.is_object()) {
            err = "the entry for " + key + " is not an object";
            return false;
        }
        Entry entry;
        if (value.contains("verdict")) {
            if (value["verdict"].is_string())
                entry.verdict = VerdictFromName(value["verdict"].get<std::string>());
            if (!entry.verdict) {
                err = "the entry for " + key + " has an unknown verdict";
                return false;
            }
        }
        if (value.contains("note") && value["note"].is_string())
            entry.note = value["note"].get<std::string>();
        std::optional<AfpPose::MatrixLerp> mode = AfpPose::MatrixLerp::Decompose;
        if (value.contains("matrix_lerp")) {
            const nlohmann::json& lerp = value["matrix_lerp"];
            mode = lerp.is_string() ? MatrixLerpFromName(lerp.get<std::string>()) : std::nullopt;
        }
        if (!mode) {
            err = "the entry for " + key + " has an unknown matrix_lerp";
            return false;
        }
        entry.matrix_lerp = *mode;
        out.emplace(key, std::move(entry));
    }
    return true;
}

}

std::string ClipKey::Text() const {
    return package + "/" + clip;
}

const char* VerdictName(Verdict verdict) {
    switch (verdict) {
    case Verdict::Smooth:
        return "smooth";
    case Verdict::Wrong:
        return "wrong";
    case Verdict::Unsure:
        return "unsure";
    }
    return "unsure";
}

std::optional<Verdict> VerdictFromName(const std::string& name) {
    for (const Verdict v : {Verdict::Smooth, Verdict::Wrong, Verdict::Unsure}) {
        if (name == VerdictName(v)) return v;
    }
    return std::nullopt;
}

std::optional<VerdictStore> VerdictStore::Open(const std::string& path, std::string& err) {
    VerdictStore store;
    store.path_ = path;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return store;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "cannot read " + path;
        return std::nullopt;
    }
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const nlohmann::json doc = nlohmann::json::parse(text, nullptr, false);
    if (doc.is_discarded()) {
        err = path + " is not valid JSON";
        return std::nullopt;
    }
    std::string why;
    if (!ReadEntries(doc, store.entries_, why)) {
        err = path + " is not a verdict file: " + why;
        return std::nullopt;
    }
    return store;
}

const Entry* VerdictStore::Find(const ClipKey& key) const {
    const auto found = entries_.find(key.Text());
    return found == entries_.end() ? nullptr : &found->second;
}

bool VerdictStore::Judged(const ClipKey& key) const {
    const Entry* entry = Find(key);
    return entry != nullptr && entry->verdict.has_value();
}

bool VerdictStore::Set(const ClipKey& key, const Entry& entry) {
    entries_[key.Text()] = entry;
    return Save();
}

bool VerdictStore::Save() {
    nlohmann::json doc;
    doc["version"] = kFormatVersion;
    nlohmann::json& verdicts = doc["verdicts"];
    verdicts = nlohmann::json::object();
    for (const auto& [key, entry] : entries_) {
        nlohmann::json value;
        if (entry.verdict) value["verdict"] = VerdictName(*entry.verdict);
        value["note"] = entry.note;
        value["matrix_lerp"] = AfpPose::MatrixLerpName(entry.matrix_lerp);
        verdicts[key] = std::move(value);
    }
    const std::filesystem::path target(path_);
    std::error_code ec;
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), ec);
    const std::filesystem::path temp = target.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            last_error_ = "cannot write " + temp.string();
            return false;
        }
        out << doc.dump(kIndent) << "\n";
        if (!out) {
            last_error_ = "writing " + temp.string() + " failed";
            return false;
        }
    }
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        last_error_ = "cannot replace " + path_ + ": " + ec.message();
        return false;
    }
    last_error_.clear();
    return true;
}

}
