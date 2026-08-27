#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Preset::Preview {

struct Sample {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgra;
};

struct Snapshot {
    std::string key;
    int total = 0;
    std::vector<Sample> samples;
    unsigned version = 0;
};

using SnapshotPtr = std::shared_ptr<const Snapshot>;

std::string KeyFor(const std::string& asset, const std::string& animation,
                   const std::vector<std::string>& hidden_parts);

class Cache {
public:
    explicit Cache(std::size_t capacity) : capacity_(capacity) {}

    [[nodiscard]] SnapshotPtr Find(const std::string& key);

    void Insert(const std::string& key, SnapshotPtr snapshot);

    void Clear();

private:
    std::size_t capacity_;
    std::vector<std::pair<std::string, SnapshotPtr>> entries_;
};

struct Request {
    std::string key;
    std::string asset;
    std::string animation;
    std::vector<std::string> hidden_parts;
    int samples = 6;
    int length = 0;
};

void Post(Request request);

bool Pump();

void Reset();

SnapshotPtr Get();

}
