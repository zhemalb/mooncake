#pragma once

#include <optional>
#include <string>
#include <vector>

#include <userver/clients/http/client.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

#include "block_store.hpp"

namespace mooncake {

// DELETE убирает блок только из памяти, на диске он остается.
// Если блока нет, GET спрашивает соседей из конфига и сохраняет найденный блок у себя.
class BlockHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-block";

    BlockHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    std::optional<Block> FetchFromPeers(const std::string& key) const;

    BlockStore& store_;
    userver::clients::http::Client& http_client_;
    std::vector<std::string> peers_;
};

}  // namespace mooncake

template <>
inline constexpr bool userver::components::kHasValidate<mooncake::BlockHandler> = true;
