#include "document/project_content.h"

#include "document/authored.h"
#include "document/keyframes.h"
#include "support/expected.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Document {

namespace {

using Json = nlohmann::ordered_json;

constexpr uint64_t kMaxDepth = 0xFFFF;

Json WriteBezier(const Bezier& bezier) {
    return Json::array({bezier.x1, bezier.y1, bezier.x2, bezier.y2});
}

Json WriteKey(const Keyframe& key) {
    Json out;
    out["frame"] = key.frame;
    out["value"] = key.value;
    out["ease"] = EaseName(key.ease);
    if (key.ease == Ease::Bezier) out["curve"] = WriteBezier(key.bezier);
    return out;
}

Json WriteTrack(const Track& track) {
    Json keys = Json::array();
    for (const Keyframe& key : track.keys)
        keys.push_back(WriteKey(key));
    Json out;
    out["property"] = track.property;
    out["keys"] = std::move(keys);
    return out;
}

Json WriteDepth(const AuthoredDepth& depth) {
    Json tracks = Json::array();
    for (const Track& track : depth.tracks)
        tracks.push_back(WriteTrack(track));
    Json out;
    out["animation"] = depth.animation;
    out["depth"] = depth.depth;
    out["first"] = depth.first_frame;
    out["last"] = depth.last_frame;
    out["tracks"] = std::move(tracks);
    if (depth.script) out["script"] = *depth.script;
    return out;
}

Support::Expected<Bezier, std::string> ReadBezier(const Json& value) {
    if (!value.is_array() || value.size() != 4)
        return Support::Unexpected(std::string("a curve is four numbers"));
    for (const Json& number : value) {
        if (!number.is_number()) return Support::Unexpected(std::string("a curve is four numbers"));
    }
    return Bezier{.x1 = value[0].get<double>(),
                  .y1 = value[1].get<double>(),
                  .x2 = value[2].get<double>(),
                  .y2 = value[3].get<double>()};
}

Support::Expected<void, std::string> ReadKeyValues(const Json& numbers, Keyframe& key) {
    for (const Json& number : numbers) {
        if (!number.is_number_integer())
            return Support::Unexpected(std::string("a keyframe value is a whole number"));
        key.value.push_back(number.get<int64_t>());
    }
    return {};
}

Support::Expected<Keyframe, std::string> ReadKey(const Json& value) {
    if (!value.is_object()) return Support::Unexpected(std::string("a keyframe is an object"));
    const auto frame = value.find("frame");
    const auto numbers = value.find("value");
    const auto ease = value.find("ease");
    if (frame == value.end() || !frame.value().is_number_unsigned())
        return Support::Unexpected(std::string("a keyframe names no frame"));
    if (numbers == value.end() || !numbers.value().is_array())
        return Support::Unexpected(std::string("a keyframe holds no values"));
    if (ease == value.end() || !ease.value().is_string())
        return Support::Unexpected(std::string("a keyframe names no ease"));
    const std::string named = ease.value().get<std::string>();
    const std::optional<Ease> known = EaseFor(named);
    if (!known) return Support::Unexpected("this editor does not know the ease " + named);

    Keyframe key{.frame = frame.value().get<uint32_t>(), .value = {}, .ease = *known, .bezier = {}};
    auto read = ReadKeyValues(numbers.value(), key);
    if (!read) return Support::Unexpected(read.error());
    const auto curve = value.find("curve");
    if (curve != value.end()) {
        auto shaped = ReadBezier(curve.value());
        if (!shaped) return Support::Unexpected(shaped.error());
        key.bezier = *shaped;
    }
    return key;
}

Support::Expected<Track, std::string> ReadTrack(const Json& value) {
    if (!value.is_object()) return Support::Unexpected(std::string("a track is an object"));
    const auto property = value.find("property");
    const auto keys = value.find("keys");
    if (property == value.end() || !property.value().is_string())
        return Support::Unexpected(std::string("a track names no property"));
    if (keys == value.end() || !keys.value().is_array())
        return Support::Unexpected(std::string("a track holds no keyframes"));
    Track track{.property = property.value().get<std::string>(), .keys = {}};
    for (const Json& key : keys.value()) {
        auto read = ReadKey(key);
        if (!read) return Support::Unexpected(read.error());
        track.keys.push_back(std::move(*read));
    }
    auto shaped = CheckTrack(track);
    if (!shaped) return Support::Unexpected(shaped.error());
    return track;
}

Support::Expected<AuthoredDepth, std::string> ReadDepth(const Json& value) {
    if (!value.is_object()) return Support::Unexpected(std::string("an owned depth is an object"));
    const auto animation = value.find("animation");
    const auto depth = value.find("depth");
    const auto first = value.find("first");
    const auto last = value.find("last");
    const auto tracks = value.find("tracks");
    if (animation == value.end() || !animation.value().is_string())
        return Support::Unexpected(std::string("an owned depth names no animation"));
    if (depth == value.end() || !depth.value().is_number_unsigned() ||
        depth.value().get<uint64_t>() > kMaxDepth) {
        return Support::Unexpected(std::string("an owned depth has no depth number"));
    }
    if (first == value.end() || !first.value().is_number_unsigned() || last == value.end() ||
        !last.value().is_number_unsigned()) {
        return Support::Unexpected(std::string("an owned depth has no frame range"));
    }
    if (tracks == value.end() || !tracks.value().is_array())
        return Support::Unexpected(std::string("an owned depth has no tracks"));

    AuthoredDepth out{.animation = animation.value().get<std::string>(),
                      .depth = depth.value().get<uint16_t>(),
                      .first_frame = first.value().get<uint32_t>(),
                      .last_frame = last.value().get<uint32_t>(),
                      .tracks = {},
                      .script = std::nullopt};
    const auto script = value.find("script");
    if (script != value.end()) {
        if (!script.value().is_string())
            return Support::Unexpected(std::string("an owned depth's script is not text"));
        out.script = script.value().get<std::string>();
    }
    if (out.first_frame > out.last_frame)
        return Support::Unexpected(std::string("an owned depth runs backwards"));
    for (const Json& track : tracks.value()) {
        auto read = ReadTrack(track);
        if (!read) return Support::Unexpected(read.error());
        out.tracks.push_back(std::move(*read));
    }
    return out;
}

}

nlohmann::ordered_json WriteContent(const std::vector<AuthoredDepth>& content) {
    Json out = Json::array();
    for (const AuthoredDepth& depth : content)
        out.push_back(WriteDepth(depth));
    return out;
}

Support::Expected<std::vector<AuthoredDepth>, std::string>
ReadContent(const nlohmann::ordered_json& value) {
    if (!value.is_array())
        return Support::Unexpected(std::string("the project's owned depths are not a list"));
    std::vector<AuthoredDepth> content;
    for (const Json& depth : value) {
        auto read = ReadDepth(depth);
        if (!read) return Support::Unexpected(read.error());
        content.push_back(std::move(*read));
    }
    return content;
}

}
