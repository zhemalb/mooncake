#include "block_store.hpp"

#include <algorithm>
#include <cctype>

#include <boost/filesystem/operations.hpp>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/fs/read.hpp>
#include <userver/fs/write.hpp>
#include <userver/utils/encoding/hex.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace mooncake {

namespace {

constexpr std::size_t kDigestSize = 32;
constexpr std::size_t kMaxKeySize = 128;

bool IsKeyChar(unsigned char c) { return std::isalnum(c) || c == '.' || c == '_' || c == '-'; }

void ValidateKey(std::string_view key) {
    if (key.empty() || key.size() > kMaxKeySize || !std::ranges::all_of(key, IsKeyChar)) {
        throw std::invalid_argument("invalid block key");
    }
}

std::string Digest(std::string_view data) {
    return userver::crypto::hash::Sha256(data, userver::crypto::hash::OutputEncoding::kBinary);
}

}  // namespace

std::string Checksum(std::string_view data) { return userver::crypto::hash::Sha256(data); }

BlockStore::BlockStore(std::string data_dir, userver::engine::TaskProcessor& fs_task_processor)
    : data_dir_(std::move(data_dir)), fs_task_processor_(fs_task_processor) {
    userver::fs::CreateDirectories(fs_task_processor_, data_dir_);
}

void BlockStore::Put(std::string_view key, std::string data, std::string_view expected_checksum) {
    ValidateKey(key);

    const auto digest = Digest(data);
    if (const auto actual = userver::utils::encoding::ToHex(digest); actual != expected_checksum) {
        throw std::invalid_argument(fmt::format("checksum mismatch: expected {}, got {}", expected_checksum, actual));
    }

    // В файле сначала sha256, потом данные.
    userver::fs::RewriteFileContentsAtomically(
        fs_task_processor_,
        BlockPath(key),
        digest + data,
        boost::filesystem::perms::owner_read | boost::filesystem::perms::owner_write
    );

    auto memory = memory_.UniqueLock();
    memory->insert_or_assign(std::string(key), std::move(data));
}

std::optional<Block> BlockStore::Get(std::string_view key) const {
    ValidateKey(key);

    {
        const auto memory = memory_.SharedLock();
        if (const auto it = memory->find(std::string(key)); it != memory->end()) {
            return Block{it->second, Tier::kRam};
        }
    }

    const auto path = BlockPath(key);
    if (!userver::fs::FileExists(fs_task_processor_, path)) {
        return std::nullopt;
    }

    auto raw = userver::fs::ReadFileContents(fs_task_processor_, path);
    if (raw.size() < kDigestSize) {
        throw CorruptedBlock(fmt::format("block {} is truncated", key));
    }

    const std::string_view stored_digest = std::string_view(raw).substr(0, kDigestSize);
    const std::string_view data = std::string_view(raw).substr(kDigestSize);
    if (Digest(data) != stored_digest) {
        throw CorruptedBlock(fmt::format("block {} checksum mismatch", key));
    }

    raw.erase(0, kDigestSize);
    return Block{std::move(raw), Tier::kSsd};
}

void BlockStore::DropFromRam(std::string_view key) {
    auto memory = memory_.UniqueLock();
    memory->erase(std::string(key));
}

std::string BlockStore::BlockPath(std::string_view key) const { return fmt::format("{}/{}.block", data_dir_, key); }

BlockStoreComponent::BlockStoreComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ComponentBase(config, context),
      store_(
          config["data-dir"].As<std::string>(),
          context.GetTaskProcessor(config["fs-task-processor"].As<std::string>())
      ) {}

userver::yaml_config::Schema BlockStoreComponent::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<ComponentBase>(R"(
type: object
description: RAM + file-backed SSD block store
additionalProperties: false
properties:
    data-dir:
        type: string
        description: directory with block files
    fs-task-processor:
        type: string
        description: task processor for file I/O
)");
}

}  // namespace mooncake
