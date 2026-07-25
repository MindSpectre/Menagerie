#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <menagerie/beavers>
#include <menagerie/crow>
#include <menagerie/spider>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <async_outcome.hpp>
#include <boost/asio/awaitable.hpp>
#include <errors.hpp>
#include <middleware.hpp>
#include <request_context.hpp>
#include <response.hpp>

namespace menagerie::http {

    class RouteRegistry;
    class HttpController;

    namespace detail {

        /// A typed error usable in a handler's Outcome: it must collapse to a
        /// Response via an ADL-found to_http_response(const E&). A missing
        /// overload fails this concept - the compile error names E.
        template <typename E>
        concept HasToHttpResponse = requires(const E& e) {
            { to_http_response(e) } -> std::same_as<Response>;
        };

        /// Maps a handler's return type onto the two supported shapes.
        template <typename R>
        struct RouteHandlerTraits {
            static constexpr bool valid = false;  ///< False: `R` matches neither supported handler shape.
        };
        /// Specialization for a handler returning AsyncResponse directly.
        template <>
        struct RouteHandlerTraits<boost::asio::awaitable<Response, Strand>> {
            static constexpr bool valid       = true;   ///< Always valid: this shape needs no error collapsing.
            static constexpr bool has_outcome = false;  ///< No Outcome to collapse; the Response is used as-is.
        };
        /// Specialization for a handler returning AsyncOutcome<Response, Es...>.
        template <typename... Es>
        struct RouteHandlerTraits<boost::asio::awaitable<beavers::Outcome<Response, Es...>, Strand>> {
            static constexpr bool valid       = (HasToHttpResponse<Es> && ...);  ///< Valid only when every Es has an ADL to_http_response.
            static constexpr bool has_outcome = true;  ///< Signals callable_route/member_outcome_route to collapse via to_http_response.
        };

        /// Collapse Outcome<Response, Es...> to Response via ADL. The exact
        /// Response&& lambda beats the template in overload resolution, so
        /// errors land in the generic branch.
        template <typename OutcomeT>
        Response collapse_outcome(OutcomeT&& outcome) {
            return std::forward<OutcomeT>(outcome).visit(
                [](Response&& r) -> Response { return std::move(r); },
                []<typename E>(E&& e) -> Response { return to_http_response(e); });
        }

        /// Compose middlewares around `inner`, right-to-left, so the FIRST
        /// added middleware is the OUTERMOST. Each layer is a plain
        /// (non-coroutine) lambda returning the middleware's awaitable
        /// directly - no wrapper frame, so the coroutine-frame budget stays
        /// at one frame per USER middleware. `next` lives in the layer
        /// closure inside the frozen chain, so the const& handed to the
        /// middleware (and held by its suspended frame) stays valid for the
        /// request's lifetime.
        ContextHandler wrap_with_middleware(ContextHandler inner, std::span<const Middleware> middlewares);

        /// The bake step: runs configure_routes() exactly once, then drains
        /// the controller's local routes into `registry` with `prefix`
        /// applied and the controller's middleware chain + Outcome collapse
        /// composed in. Called by GroupBinding and by Server::add_controller.
        /// @throw std::invalid_argument if `ctrl` is null.
        /// @throw std::logic_error on a second bake of the same controller.
        struct ControllerBaker {
            /// Runs `ctrl`'s configure_routes() once, then drains its local
            /// routes into `registry` with `prefix` and middleware/Outcome
            /// collapse composed in.
            static void
            bake_into(RouteRegistry& registry, const std::shared_ptr<HttpController>& ctrl, std::string_view prefix);
        };

    }  // namespace detail

    /// A callable route handler: invocable with a RequestContext (by value),
    /// returning AsyncResponse or AsyncOutcome<Response, Es...> where every E
    /// has an ADL to_http_response.
    template <typename F>
    concept IsRouteHandler = std::invocable<std::decay_t<F>&, RequestContext> &&
                             detail::RouteHandlerTraits<std::invoke_result_t<std::decay_t<F>&, RequestContext>>::valid;

    /**
     * @brief Application base class. Subclasses register routes in
     *        configure_routes() via the protected verb DSL; GroupBinding bakes
     *        them into the server-wide registry with prefix + middleware +
     *        Outcome-to-Response conversion pre-composed.
     *
     * Lifecycle: construct, then add_middleware()*, then bake (via
     * GroupBinding::add_controller, which calls configure_routes() once),
     * then frozen. Registration or add_middleware after bake throws.
     */
    class HttpController : public std::enable_shared_from_this<HttpController> {
    public:
        /// Declares this controller's Spider registry lifetime policy as Resettable.
        SPIDER_WEB(spider::Resettable);

        virtual ~HttpController() = default;

        /// Populate the local route table via the verb DSL. Called exactly
        /// once by the bake step - do not call it yourself.
        /// Controllers manage their resources via RAII (ctor acquires, dtor
        /// releases) - there are no framework lifecycle hooks. For async
        /// cleanup that must run while the executor is still driven, use a
        /// ServerObserver's on_shutdown_started().
        virtual void configure_routes() = 0;

        /// Attaches a middleware; middlewares run in ADDITION ORDER (first
        /// added = outermost).
        /// @throw std::logic_error if called after bake.
        template <typename Mw>
            requires std::constructible_from<Middleware, Mw&&>
        HttpController& add_middleware(Mw&& mw) {
            if (baked_)
                throw std::logic_error{"HttpController: add_middleware after bake"};
            middlewares_.emplace_back(std::forward<Mw>(mw));
            return *this;
        }

    protected:
        // -- Verb DSL: 3 shapes x 7 verbs --
        //
        // Each verb (Get, Post, Put, Patch, Delete, Head, Options) has three
        // overloads: a member function pointer returning AsyncResponse; a
        // member function pointer returning AsyncOutcome<Response, Es...>
        // (each Es collapsed to a Response via its to_http_response overload);
        // or any free function/lambda satisfying IsRouteHandler in either
        // return shape.

        // --GET--
        /// Registers a GET route at `path` calling member function `m`.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerTp>
        void Get(StringTp&& path, AsyncResponse (ControllerTp::*m)(RequestContext)) {
            member_route(HttpMethod::get, std::forward<StringTp>(path), m);
        }

        /// Registers a GET route at `path` calling member function `m`,
        /// collapsing its typed-error Outcome result.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
        void Get(StringTp&& path, AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::get, std::forward<StringTp>(path), m);
        }

        /// Registers a GET route at `path` calling free function/lambda `f`.
        template <beavers::IsStringLike StringTp, IsRouteHandler FuncTp>
        void Get(StringTp&& path, FuncTp&& f) {
            callable_route(HttpMethod::get, std::forward<StringTp>(path), std::forward<FuncTp>(f));
        }

        // --POST--
        /// Registers a POST route at `path` calling member function `m`.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerTp>
        void Post(StringTp&& path, AsyncResponse (ControllerTp::*m)(RequestContext)) {
            member_route(HttpMethod::post, std::forward<StringTp>(path), m);
        }

        /// Registers a POST route at `path` calling member function `m`,
        /// collapsing its typed-error Outcome result.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
        void Post(StringTp&& path, AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::post, std::forward<StringTp>(path), m);
        }

        /// Registers a POST route at `path` calling free function/lambda `f`.
        template <beavers::IsStringLike StringTp, IsRouteHandler FuncTp>
        void Post(StringTp&& path, FuncTp&& f) {
            callable_route(HttpMethod::post, std::forward<StringTp>(path), std::forward<FuncTp>(f));
        }

        // --PUT--
        /// Registers a PUT route at `path` calling member function `m`.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerTp>
        void Put(StringTp&& path, AsyncResponse (ControllerTp::*m)(RequestContext)) {
            member_route(HttpMethod::put, std::forward<StringTp>(path), m);
        }

        /// Registers a PUT route at `path` calling member function `m`,
        /// collapsing its typed-error Outcome result.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
        void Put(StringTp&& path, AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::put, std::forward<StringTp>(path), m);
        }

        /// Registers a PUT route at `path` calling free function/lambda `f`.
        template <beavers::IsStringLike StringTp, IsRouteHandler FuncTp>
        void Put(StringTp&& path, FuncTp&& f) {
            callable_route(HttpMethod::put, std::forward<StringTp>(path), std::forward<FuncTp>(f));
        }

        // --PATCH--
        /// Registers a PATCH route at `path` calling member function `m`.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerTp>
        void Patch(StringTp&& path, AsyncResponse (ControllerTp::*m)(RequestContext)) {
            member_route(HttpMethod::patch, std::forward<StringTp>(path), m);
        }

        /// Registers a PATCH route at `path` calling member function `m`,
        /// collapsing its typed-error Outcome result.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
        void Patch(StringTp&& path, AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::patch, std::forward<StringTp>(path), m);
        }

        /// Registers a PATCH route at `path` calling free function/lambda `f`.
        template <beavers::IsStringLike StringTp, IsRouteHandler FuncTp>
        void Patch(StringTp&& path, FuncTp&& f) {
            callable_route(HttpMethod::patch, std::forward<StringTp>(path), std::forward<FuncTp>(f));
        }

        // --DELETE--
        /// Registers a DELETE route at `path` calling member function `m`.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerTp>
        void Delete(StringTp&& path, AsyncResponse (ControllerTp::*m)(RequestContext)) {
            member_route(HttpMethod::del, std::forward<StringTp>(path), m);
        }

        /// Registers a DELETE route at `path` calling member function `m`,
        /// collapsing its typed-error Outcome result.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
        void Delete(StringTp&& path, AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::del, std::forward<StringTp>(path), m);
        }

        /// Registers a DELETE route at `path` calling free function/lambda `f`.
        template <beavers::IsStringLike StringTp, IsRouteHandler FuncTp>
        void Delete(StringTp&& path, FuncTp&& f) {
            callable_route(HttpMethod::del, std::forward<StringTp>(path), std::forward<FuncTp>(f));
        }

        // --HEAD--
        /// Registers a HEAD route at `path` calling member function `m`.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerTp>
        void Head(StringTp&& path, AsyncResponse (ControllerTp::*m)(RequestContext)) {
            member_route(HttpMethod::head, std::forward<StringTp>(path), m);
        }

        /// Registers a HEAD route at `path` calling member function `m`,
        /// collapsing its typed-error Outcome result.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
        void Head(StringTp&& path, AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::head, std::forward<StringTp>(path), m);
        }

        /// Registers a HEAD route at `path` calling free function/lambda `f`.
        template <beavers::IsStringLike StringTp, IsRouteHandler FuncTp>
        void Head(StringTp&& path, FuncTp&& f) {
            callable_route(HttpMethod::head, std::forward<StringTp>(path), std::forward<FuncTp>(f));
        }

        // --OPTIONS--
        /// Registers an OPTIONS route at `path` calling member function `m`.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerTp>
        void Options(StringTp&& path, AsyncResponse (ControllerTp::*m)(RequestContext)) {
            member_route(HttpMethod::options, std::forward<StringTp>(path), m);
        }

        /// Registers an OPTIONS route at `path` calling member function `m`,
        /// collapsing its typed-error Outcome result.
        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
        void Options(StringTp&& path, AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::options, std::forward<StringTp>(path), m);
        }

        /// Registers an OPTIONS route at `path` calling free function/lambda `f`.
        template <beavers::IsStringLike StringTp, IsRouteHandler FuncTp>
        void Options(StringTp&& path, FuncTp&& f) {
            callable_route(HttpMethod::options, std::forward<StringTp>(path), std::forward<FuncTp>(f));
        }

        // TODO(HTTP RFC 10008) Query method
    private:
        friend struct detail::ControllerBaker;

        using BakeFn =
            std::function<ContextHandler(const std::shared_ptr<HttpController>&, std::span<const Middleware>)>;
        struct LocalRoute {
            HttpMethod method;
            std::string path;
            BakeFn bake;
        };

        template <beavers::IsStringLike StringTp, typename BakeFnTp>
        void push_route(const HttpMethod method, StringTp&& path, BakeFnTp&& bake) {
            if (baked_)
                throw std::logic_error{"HttpController: route registration after bake"};
            local_routes_.emplace_back(method, std::forward<StringTp>(path), std::forward<BakeFnTp>(bake));
        }


        template <std::derived_from<HttpController> ControllerT>
        static std::shared_ptr<ControllerT> typed_self(const std::shared_ptr<HttpController>& self) {
            // Bake-time only - never on the request path (runtime dynamic_cast
            // is forbidden there). Guards Get("/x", &OtherController::h)
            // cross-registration with a startup error instead of UB.
            auto typed = std::dynamic_pointer_cast<ControllerT>(self);
            if (!typed)
                throw std::logic_error{"HttpController: registered member function does not "
                                       "belong to the baked controller type"};
            return typed;
        }

        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT>
        void member_route(const HttpMethod method, StringTp&& path, AsyncResponse (ControllerT::*m)(RequestContext)) {
            push_route(method,
                       std::forward<StringTp>(path),
                       [m](const std::shared_ptr<HttpController>& self,
                           const std::span<const Middleware> mws) -> ContextHandler {
                           // Plain lambda returning the member coroutine's
                           // awaitable directly - no wrapper frame. `typed`
                           // keeps the controller alive in the closure, which
                           // lives in the frozen registry.
                           ContextHandler inner = [typed = typed_self<ControllerT>(self),
                                                   m](RequestContext ctx) -> AsyncResponse {
                               return (typed.get()->*m)(std::move(ctx));
                           };
                           return detail::wrap_with_middleware(std::move(inner), mws);
                       });
        }

        template <beavers::IsStringLike StringTp, std::derived_from<HttpController> ControllerT, typename... Es>
            requires(detail::HasToHttpResponse<Es> && ...)
        void member_outcome_route(const HttpMethod method,
                                  StringTp&& path,
                                  AsyncOutcome<Response, Es...> (ControllerT::*m)(RequestContext)) {
            push_route(method,
                       std::forward<StringTp>(path),
                       [m](const std::shared_ptr<HttpController>& self,
                           const std::span<const Middleware> mws) -> ContextHandler {
                           ContextHandler inner = [typed = typed_self<ControllerT>(self),
                                                   m](RequestContext ctx) -> AsyncResponse {
                               auto outcome = co_await (typed.get()->*m)(std::move(ctx));
                               co_return detail::collapse_outcome(std::move(outcome));
                           };
                           return detail::wrap_with_middleware(std::move(inner), mws);
                       });
        }

        template <beavers::IsStringLike StringTp, IsRouteHandler HandlerFunctionT>
        void callable_route(const HttpMethod method, StringTp&& path, HandlerFunctionT&& f) {
            using Fn     = std::decay_t<HandlerFunctionT>;
            using Traits = detail::RouteHandlerTraits<std::invoke_result_t<Fn&, RequestContext>>;
            push_route(method,
                       std::forward<StringTp>(path),
                       [f = Fn{std::forward<HandlerFunctionT>(f)}](
                           const std::shared_ptr<HttpController>&,
                           const std::span<const Middleware> mws) mutable -> ContextHandler {
                           ContextHandler inner;
                           if constexpr (Traits::has_outcome) {
                               inner = [f](RequestContext ctx) mutable -> AsyncResponse {
                                   auto outcome = co_await f(std::move(ctx));
                                   co_return detail::collapse_outcome(std::move(outcome));
                               };
                           } else {
                               inner = ContextHandler{std::move(f)};  // signature matches exactly
                           }
                           return detail::wrap_with_middleware(std::move(inner), mws);
                       });
        }

        bool configured_ = false;
        bool baked_      = false;
        std::vector<LocalRoute> local_routes_;
        std::vector<Middleware> middlewares_;

        SCROLL_COMPONENT_PREFIX("HttpController");
    };

}  // namespace menagerie::http
