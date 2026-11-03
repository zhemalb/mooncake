#include "block_store.hpp"

#include <userver/engine/task/current_task.hpp>
#include <userver/fs/blocking/read.hpp>
#include <userver/fs/blocking/temp_directory.hpp>
#include <userver/fs/blocking/write.hpp>
#include <userver/utest/utest.hpp>

namespace mooncake {
namespace {

namespace blocking = userver::fs::blocking;

class Fixture {
public:
    BlockStore& store() { return store_; }

private:
    blocking::TempDirectory dir_ = blocking::TempDirectory::Create();
    BlockStore store_{dir_.GetPath(), userver::engine::current_task::GetTaskProcessor()};
};

UTEST(BlockStore, ReadsFromRamThenFromSsd) {
    Fixture f;
    const std::string payload = "hello";
    f.store().Put("block-1", payload, Checksum(payload));

    auto block = f.store().Get("block-1");
    ASSERT_TRUE(block);
    EXPECT_EQ(block->tier, Tier::kRam);
    EXPECT_EQ(block->data, payload);

    f.store().DropFromRam("block-1");
    block = f.store().Get("block-1");
    ASSERT_TRUE(block);
    EXPECT_EQ(block->tier, Tier::kSsd);
    EXPECT_EQ(block->data, payload);
}

UTEST(BlockStore, MissingBlockIsNullopt) {
    Fixture f;
    EXPECT_FALSE(f.store().Get("absent"));
}

// Ключ с запрещенными символами, слишком длинный ключ и неверная сумма.
UTEST(BlockStore, RejectsBadKeyAndChecksum) {
    Fixture f;
    EXPECT_THROW(f.store().Put("../escape", "x", Checksum("x")), std::invalid_argument);
    EXPECT_THROW(f.store().Put(std::string(300, 'a'), "x", Checksum("x")), std::invalid_argument);
    EXPECT_THROW(f.store().Put("block-1", "corrupted", Checksum("expected")), std::invalid_argument);
    EXPECT_FALSE(f.store().Get("block-1"));
}

// Портим файл на диске, потом обрезаем его, и в обоих случаях блок не отдается.
UTEST(BlockStore, RejectsCorruptedSsdBlock) {
    Fixture f;
    const std::string payload = "hello";
    f.store().Put("block-1", payload, Checksum(payload));
    f.store().DropFromRam("block-1");

    const auto path = f.store().BlockPath("block-1");
    auto raw = blocking::ReadFileContents(path);
    raw.back() ^= 0x7f;
    blocking::RewriteFileContents(path, raw);
    EXPECT_THROW(f.store().Get("block-1"), CorruptedBlock);

    blocking::RewriteFileContents(path, raw.substr(0, 10));
    EXPECT_THROW(f.store().Get("block-1"), CorruptedBlock);
}

}  // namespace
}  // namespace mooncake
