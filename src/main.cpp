#include <userver/clients/http/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/utils/daemon_run.hpp>

#include "block_handler.hpp"
#include "block_store.hpp"

int main(int argc, char* argv[]) {
    const auto components =
        userver::components::MinimalServerComponentList()
            .AppendComponentList(userver::clients::http::ComponentList())
            .Append<userver::server::handlers::Ping>()
            .Append<mooncake::BlockStoreComponent>()
            .Append<mooncake::BlockHandler>();

    return userver::utils::DaemonMain(argc, argv, components);
}
