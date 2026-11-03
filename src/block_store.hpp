#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include <userver/components/component_base.hpp>
#include <userver/concurrent/variable.hpp>
#include <userver/engine/shared_mutex.hpp>
#include <userver/engine/task/task_processor_fwd.hpp>

namespace mooncake {

enum class Tier { kRam, kSsd, kRemote };

constexpr std::string_view ToString(Tier tier) {
    switch (tier) {
        case Tier::kRam:
            return "ram";
        case Tier::kSsd:
            return "ssd";
        case Tier::kRemote:
            return "remote";
    }
    return "unknown";
}

struct Block {
    std::string data;
    Tier tier;
};

class CorruptedBlock : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::string Checksum(std::string_view data);

// Пока память и диск растут без ограничений, LRU добавим к КТ-2.
class BlockStore final {
public:
    BlockStore(std::string data_dir, userver::engine::TaskProcessor& fs_task_processor);

    void Put(std::string_view key, std::string data, std::string_view expected_checksum);
    std::optional<Block> Get(std::string_view key) const;
    void DropFromRam(std::string_view key);

    std::string BlockPath(std::string_view key) const;

private:
    using Memory = std::unordered_map<std::string, std::string>;

    std::string data_dir_;
    userver::engine::TaskProcessor& fs_task_processor_;
    userver::concurrent::Variable<Memory, userver::engine::SharedMutex> memory_;
};

class BlockStoreComponent final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "block-store";

    BlockStoreComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    BlockStore& GetStore() { return store_; }

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    BlockStore store_;
};

}  // namespace mooncake

template <>
inline constexpr bool userver::components::kHasValidate<mooncake::BlockStoreComponent> = true;
