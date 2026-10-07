#include "encode/frame_section.h"

#include "support/expected.h"

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/windows_shared_memory.hpp>

#include <cstddef>
#include <format>
#include <memory>
#include <string>

namespace EncodeFrames {

namespace ipc = boost::interprocess;

Section::Section(const std::string& name, std::size_t bytes)
    : memory_(ipc::create_only, name.c_str(), ipc::read_write, bytes),
      region_(memory_, ipc::read_write) {}

Section::Section(const std::string& name)
    : memory_(ipc::open_only, name.c_str(), ipc::read_write), region_(memory_, ipc::read_write) {}

Support::Expected<std::unique_ptr<Section>, std::string> Section::Create(const std::string& name,
                                                                         std::size_t bytes) {
    try {
        return std::make_unique<Section>(name, bytes);
    } catch (const ipc::interprocess_exception& error) {
        return Support::Unexpected(std::format("cannot create {}: {}", name, error.what()));
    }
}

Support::Expected<std::unique_ptr<Section>, std::string> Section::Open(const std::string& name) {
    try {
        return std::make_unique<Section>(name);
    } catch (const ipc::interprocess_exception& error) {
        return Support::Unexpected(std::format("cannot open {}: {}", name, error.what()));
    }
}

}
