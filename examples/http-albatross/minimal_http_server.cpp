/**
 * Minimal menagerie HTTP server (standalone path).
 *
 * Two wiring paths:
 *   ./minimal_http_server                 → programmatic config, 127.0.0.1:8080
 *   ./minimal_http_server server.json     → JSON config via load_server_config
 *                                           + attach_default_listeners
 * Try:
 *   curl http://127.0.0.1:8080/api/hello/world
 *   curl http://127.0.0.1:8080/api/healthz
 *   curl -d 'payload' http://127.0.0.1:8080/api/echo
 *
 * Ctrl+C triggers graceful shutdown (accept loops cancelled, in-flight
 * requests drained, observers notified) — run_standalone owns the executor,
 * the worker threads, and the stop -> wait -> teardown sequence.
 */
#include <cstddef>
#include <expected>
#include <iostream>
#include <memory>
#include <menagerie/albatross>
#include <menagerie/beaver>
#include <string>
#include <utility>
#include <variant>

namespace {

    using namespace menagerie::beaver::literals;

    namespace http = menagerie::albatross;

    /// GET /hello/{name} — path parameter, URL-decoded into the request arena.
    /// GET /healthz      — arena-backed JSON response.
    /// POST /echo        — typed-error handler: AsyncOutcome + ADL to_http_response.
    class GreeterController final : public http::HttpController {
    public:
        void configure_routes() override {
            Get("/hello/{name}", &GreeterController::hello);
            Get("/healthz", &GreeterController::healthz);
            Post("/echo", &GreeterController::echo);
        }

    private:
        static http::AsyncResponse hello(http::RequestContext ctx) {
            co_return ctx.ok("hello, " + ctx.path_param_or<std::string>("name", std::string{"world"}) + "\n");
        }

        static http::AsyncResponse healthz(http::RequestContext ctx) {
            co_return ctx.json(R"({"status":"ok"})");
        }

        /// Oversize bodies short-circuit as BodyLimitExceeded; the bake layer
        /// converts the typed error into a 413 via errors.hpp's ADL
        /// to_http_response — the handler never builds an error response.
        static http::AsyncOutcome<http::Response, http::BodyLimitExceeded> echo(http::RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(64_kb);
            if (!body) {
                co_return std::unexpected(std::move(body).error());
            }
            co_return ctx.ok(std::move(body).value());
        }
    };

    /// Post-processing middleware: stamps a header on every response of the
    /// controller it is attached to (runs after the handler returns).
    http::Middleware server_tag_middleware() {
        return [](http::RequestContext ctx, const http::NextHandler& next) -> http::AsyncResponse {
            auto response = co_await next(std::move(ctx));
            response.headers.set("X-Example", "minimal-http-server");
            co_return response;
        };
    }

    /// Report whichever config error alternative `loaded` holds.
    [[nodiscard]] int report_config_error(
        const std::expected<http::ServerConfig,
                            std::variant<http::ConfigFileError, http::ConfigParseError, http::ConfigSchemaError>>&
            loaded) {
        std::visit(menagerie::beaver::overloaded{
                       [](const http::ConfigFileError& e) {
                           std::cerr << "config: cannot read " << e.path << ": " << e.reason << '\n';
                       },
                       [](const http::ConfigParseError& e) {
                           std::cerr << "config: parse error at " << e.path << ':' << e.line << ": " << e.detail
                                     << '\n';
                       },
                       [](const http::ConfigSchemaError& e) {
                           std::cerr << "config: invalid value in " << e.path << ": " << e.detail << '\n';
                       },
                   },
                   loaded.error());
        return 1;
    }

}  // namespace

int main(const int argc, char* argv[]) {
    namespace http = menagerie::albatross;

    http::ServerConfig cfg = http::ServerConfig::Builder{}.finalize();
    if (argc > 1) {
        auto loaded = http::load_server_config(argv[1]);
        if (!loaded) {
            return report_config_error(loaded);
        }
        cfg = std::move(loaded).value();
    }

    const std::size_t threads = cfg.threads();
    std::cout << "minimal_http_server: " << threads << " io threads — Ctrl+C for graceful shutdown\n";

    try {
        http::run_standalone(std::move(cfg), threads, [](http::Server& server) {
            // JSON-declared listeners (no-op when the config has none) …
            http::attach_default_listeners(server);
            // … falling back to a programmatic one so both paths just work.
            if (server.listeners().empty()) {
                server.add_tcp_listener("127.0.0.1", 8080, http::Http11Driver{http::Http11Config{}});
            }

            auto greeter = std::make_shared<GreeterController>();
            greeter->add_middleware(server_tag_middleware());
            server.in_group("/api").add_controller(std::move(greeter));
        });
    } catch (const std::exception& e) {
        std::cerr << "minimal_http_server: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
