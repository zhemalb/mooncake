#include "block_handler.hpp"

#include <chrono>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/http/predefined_header.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace mooncake {

namespace {

namespace handlers = userver::server::handlers;
using userver::http::headers::PredefinedHeader;
using userver::server::http::HttpMethod;
using userver::server::http::HttpStatus;

constexpr PredefinedHeader kChecksumHeader{"X-Checksum-SHA256"};
constexpr PredefinedHeader kSourceHeader{"X-Block-Source"};
// Запрос от соседа, такой узел к своим соседям уже не ходит.
constexpr PredefinedHeader kFromPeerHeader{"X-From-Peer"};

constexpr std::chrono::milliseconds kPeerTimeout{1000};

}  // namespace

BlockHandler::BlockHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerBase(config, context),
      store_(context.FindComponent<BlockStoreComponent>().GetStore()),
      http_client_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
      peers_(config["peers"].As<std::vector<std::string>>({})) {}

std::string BlockHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    const auto& key = request.GetPathArg("key");
    auto& response = request.GetHttpResponse();

    try {
        switch (request.GetMethod()) {
            case HttpMethod::kPut:
                store_.Put(key, request.RequestBody(), request.GetHeader(kChecksumHeader));
                response.SetStatus(HttpStatus::kCreated);
                return {};

            case HttpMethod::kGet: {
                auto block = store_.Get(key);
                if (!block && !request.HasHeader(kFromPeerHeader)) {
                    block = FetchFromPeers(key);
                }
                if (!block) {
                    throw handlers::ResourceNotFound();
                }
                response.SetHeader(kSourceHeader, std::string(ToString(block->tier)));
                response.SetHeader(kChecksumHeader, Checksum(block->data));
                response.SetContentType("application/octet-stream");
                return std::move(block->data);
            }

            case HttpMethod::kDelete:
                store_.DropFromRam(key);
                response.SetStatus(HttpStatus::kNoContent);
                return {};

            default:
                throw handlers::ClientError();
        }
    } catch (const std::invalid_argument& e) {
        throw handlers::ClientError(handlers::ExternalBody{e.what()});
    } catch (const CorruptedBlock& e) {
        throw handlers::InternalServerError(handlers::ExternalBody{e.what()});
    }
}

std::optional<Block> BlockHandler::FetchFromPeers(const std::string& key) const {
    for (const auto& peer : peers_) {
        try {
            auto request = http_client_.CreateRequest();
            request.get(peer + "/blocks/" + key).headers({{kFromPeerHeader, "1"}}).timeout(kPeerTimeout);
            const auto peer_response = request.perform();
            if (!peer_response->IsOk()) {
                continue;
            }

            auto data = peer_response->body();
            const auto& checksum = peer_response->headers().at(std::string{kChecksumHeader});
            // Put сам сверит сумму, испорченный блок не сохранится.
            store_.Put(key, data, checksum);
            return Block{std::move(data), Tier::kRemote};
        } catch (const std::exception&) {
            // Сосед недоступен или прислал испорченный блок, пробуем следующего.
        }
    }
    return std::nullopt;
}

userver::yaml_config::Schema BlockHandler::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HttpHandlerBase>(R"(
type: object
description: GET, PUT and DELETE for blocks
additionalProperties: false
properties:
    peers:
        type: array
        description: base URLs of other cache nodes
        items:
            type: string
            description: base URL of a cache node
)");
}

}  // namespace mooncake
