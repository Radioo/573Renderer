#pragma once

#include "support/expected.h"

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/windows_shared_memory.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace EncodeFrames {

class Section {
public:
    Section(const std::string& name, std::size_t bytes);
    explicit Section(const std::string& name);
    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;
    Section(Section&&) = delete;
    Section& operator=(Section&&) = delete;
    ~Section() = default;

    [[nodiscard]] static Support::Expected<std::unique_ptr<Section>, std::string>
    Create(const std::string& name, std::size_t bytes);
    [[nodiscard]] static Support::Expected<std::unique_ptr<Section>, std::string>
    Open(const std::string& name);

    [[nodiscard]] uint8_t* Data() const { return static_cast<uint8_t*>(region_.get_address()); }

private:
    boost::interprocess::windows_shared_memory memory_;
    boost::interprocess::mapped_region region_;
};

}
