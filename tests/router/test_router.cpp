#include <iostream>
#include <cassert>
#include <utility>
#include "context.hpp"
#include "router.hpp"

/* Mock request handlers */
Gecko::HttpResponse handlerHome(const Gecko::HttpRequest &req) {
    auto response = Gecko::HttpResponse::stockResponse(200);
    response.setBody("Home Page");
    return response;
}

Gecko::HttpResponse handlerUsers(const Gecko::HttpRequest &req) {
    auto response = Gecko::HttpResponse::stockResponse(200);
    response.setBody("Users List");
    return response;
}

Gecko::HttpResponse handlerUserDetail(const Gecko::HttpRequest &req) {
    auto response = Gecko::HttpResponse::stockResponse(200);
    response.setBody("User Detail");
    return response;
}

Gecko::HttpResponse handlerUserPosts(const Gecko::HttpRequest &req) {
    auto response = Gecko::HttpResponse::stockResponse(200);
    response.setBody("User Posts");
    return response;
}

Gecko::HttpResponse handlerCreateUser(const Gecko::HttpRequest &req) {
    auto response = Gecko::HttpResponse::stockResponse(201);
    response.setBody("User Created");
    return response;
}

Gecko::HttpResponse handlerUpdateUser(const Gecko::HttpRequest &req) {
    auto response = Gecko::HttpResponse::stockResponse(200);
    response.setBody("User Updated");
    return response;
}

template <typename Handler>
Gecko::RequestHandler wrap_response_handler(Handler&& handler) {
    return [handler = std::forward<Handler>(handler)](Gecko::Context& ctx) {
        ctx.response() = handler(ctx.request());
    };
}

void test_split_path() {
    /* Root path */
    auto result1 = Gecko::split_path("/");
    assert(result1.empty());
    
    /* Simple path */
    auto result2 = Gecko::split_path("/users");
    assert(result2.size() == 1);
    assert(result2[0] == "users");
    
    /* Complex path */
    auto result3 = Gecko::split_path("/users/:id/posts");
    assert(result3.size() == 3);
    assert(result3[0] == "users");
    assert(result3[1] == ":id");
    assert(result3[2] == "posts");
    
    /* Path with redundant slashes */
    auto result4 = Gecko::split_path("///users//posts///");
    assert(result4.size() == 2);
    assert(result4[0] == "users");
    assert(result4[1] == "posts");
    
}

void test_static_routes() {
    Gecko::Router router;
    
    /* Register static routes */
    router.insert(Gecko::HttpMethod::GET, "/", wrap_response_handler(handlerHome));
    router.insert(Gecko::HttpMethod::GET, "/users", wrap_response_handler(handlerUsers));
    router.insert(Gecko::HttpMethod::POST, "/users", wrap_response_handler(handlerCreateUser));
    router.insert(Gecko::HttpMethod::GET, "/users/profile", wrap_response_handler(handlerUserDetail));
    
    /* GET / route */
    auto result1 = router.find(Gecko::HttpMethod::GET, "/");
    assert(result1.has_value());
    assert(result1->handler != nullptr);
    assert(result1->params.empty());
    
    /* GET /users route */
    auto result2 = router.find(Gecko::HttpMethod::GET, "/users");
    assert(result2.has_value());
    assert(result2->handler != nullptr);
    assert(result2->params.empty());
    
    /* POST /users route */
    auto result3 = router.find(Gecko::HttpMethod::POST, "/users");
    assert(result3.has_value());
    assert(result3->handler != nullptr);
    assert(result3->params.empty());
    
    /* Nested path */
    auto result4 = router.find(Gecko::HttpMethod::GET, "/users/profile");
    assert(result4.has_value());
    assert(result4->handler != nullptr);
    assert(result4->params.empty());
    
    /* Missing route */
    auto result5 = router.find(Gecko::HttpMethod::GET, "/nonexistent");
    assert(!result5.has_value());
    
    /* Missing method */
    auto result6 = router.find(Gecko::HttpMethod::DELETE, "/users");
    assert(!result6.has_value());
    
}

void test_param_routes() {
    Gecko::Router router;
    
    /* Register parameter routes */
    router.insert(Gecko::HttpMethod::GET, "/users/:id", wrap_response_handler(handlerUserDetail));
    router.insert(Gecko::HttpMethod::PUT, "/users/:id", wrap_response_handler(handlerUpdateUser));
    router.insert(Gecko::HttpMethod::GET, "/users/:id/posts", wrap_response_handler(handlerUserPosts));
    
    /* Single parameter */
    auto result1 = router.find(Gecko::HttpMethod::GET, "/users/123");
    assert(result1.has_value());
    assert(result1->handler != nullptr);
    assert(result1->params.size() == 1);
    assert(result1->params["id"] == "123");
    
    /* Different parameter values */
    auto result2 = router.find(Gecko::HttpMethod::GET, "/users/abc");
    assert(result2.has_value());
    assert(result2->params["id"] == "abc");
    
    /* Same path different method */
    auto result3 = router.find(Gecko::HttpMethod::PUT, "/users/456");
    assert(result3.has_value());
    assert(result3->params["id"] == "456");
    
    /* Nested parameter route */
    auto result4 = router.find(Gecko::HttpMethod::GET, "/users/789/posts");
    assert(result4.has_value());
    assert(result4->params["id"] == "789");
    
    /* Parameter mismatch */
    auto result5 = router.find(Gecko::HttpMethod::GET, "/users");
    assert(!result5.has_value());
    
    auto result6 = router.find(Gecko::HttpMethod::GET, "/users/123/comments");
    assert(!result6.has_value());
    
}

void test_mixed_routes() {
    Gecko::Router router;
    
    /* Register mixed routes */
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users", wrap_response_handler(handlerUsers));
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users/:id", wrap_response_handler(handlerUserDetail));
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users/:id/posts", wrap_response_handler(handlerUserPosts));
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users/profile", wrap_response_handler(handlerUserDetail));
    
    /* Static routes take priority */
    auto result1 = router.find(Gecko::HttpMethod::GET, "/api/v1/users/profile");
    assert(result1.has_value());
    assert(result1->params.empty());  /* Static route has no params */
    
    /* Parameter route */
    auto result2 = router.find(Gecko::HttpMethod::GET, "/api/v1/users/123");
    assert(result2.has_value());
    assert(result2->params["id"] == "123");
    
    /* Nested mixed route */
    auto result3 = router.find(Gecko::HttpMethod::GET, "/api/v1/users/456/posts");
    assert(result3.has_value());
    assert(result3->params["id"] == "456");
    
    /* Collection route */
    auto result4 = router.find(Gecko::HttpMethod::GET, "/api/v1/users");
    assert(result4.has_value());
    assert(result4->params.empty());
    
}

void test_multiple_params() {
    Gecko::Router router;
    
    /* Register multi-parameter routes */
    router.insert(Gecko::HttpMethod::GET, "/users/:userId/posts/:postId", 
                  wrap_response_handler([](const Gecko::HttpRequest &req) -> Gecko::HttpResponse {
                      auto response = Gecko::HttpResponse::stockResponse(200);
                      response.setBody("User Post Detail");
                      return response;
                  }));
    
    router.insert(Gecko::HttpMethod::GET, "/api/:version/users/:id/settings/:key",
                  wrap_response_handler([](const Gecko::HttpRequest &req) -> Gecko::HttpResponse {
                      auto response = Gecko::HttpResponse::stockResponse(200);
                      response.setBody("User Setting");
                      return response;
                  }));
    
    /* Two-parameter route */
    auto result1 = router.find(Gecko::HttpMethod::GET, "/users/123/posts/456");
    assert(result1.has_value());
    assert(result1->params.size() == 2);
    assert(result1->params["userId"] == "123");
    assert(result1->params["postId"] == "456");
    
    /* Multi-parameter route */
    auto result2 = router.find(Gecko::HttpMethod::GET, "/api/v2/users/789/settings/theme");
    assert(result2.has_value());
    assert(result2->params.size() == 3);
    assert(result2->params["version"] == "v2");
    assert(result2->params["id"] == "789");
    assert(result2->params["key"] == "theme");
    
    /* Missing parameter path */
    auto result3 = router.find(Gecko::HttpMethod::GET, "/users/123/posts");
    assert(!result3.has_value());
    
}

void test_route_conflicts() {
    Gecko::Router router;
    
    /* Route override scenario */
    router.insert(Gecko::HttpMethod::GET, "/users/:id", wrap_response_handler(handlerUserDetail));
    router.insert(Gecko::HttpMethod::GET, "/users/:id", wrap_response_handler(handlerUsers));  /* Override previous handler */
    
    auto result1 = router.find(Gecko::HttpMethod::GET, "/users/123");
    assert(result1.has_value());
    /* Last inserted handler should win */
    
    /* Different methods stay isolated */
    router.insert(Gecko::HttpMethod::POST, "/users/:id", wrap_response_handler(handlerCreateUser));
    router.insert(Gecko::HttpMethod::PUT, "/users/:id", wrap_response_handler(handlerUpdateUser));
    
    auto result2 = router.find(Gecko::HttpMethod::POST, "/users/123");
    assert(result2.has_value());
    
    auto result3 = router.find(Gecko::HttpMethod::PUT, "/users/123");
    assert(result3.has_value());
    
}

void test_edge_cases() {
    Gecko::Router router;
    
    /* Empty path */
    router.insert(Gecko::HttpMethod::GET, "", wrap_response_handler(handlerHome));
    auto result1 = router.find(Gecko::HttpMethod::GET, "");
    assert(result1.has_value());
    
    /* Slash-only path */
    router.insert(Gecko::HttpMethod::GET, "/", wrap_response_handler(handlerHome));
    auto result2 = router.find(Gecko::HttpMethod::GET, "/");
    assert(result2.has_value());
    
    /* Empty parameter name */
    router.insert(Gecko::HttpMethod::GET, "/test/:", wrap_response_handler(handlerUserDetail));
    auto result3 = router.find(Gecko::HttpMethod::GET, "/test/value");
    assert(result3.has_value());
    assert(result3->params[""] == "value");  /* Empty parameter key */
    
    /* Long path */
    std::string longPath = "/api";
    for (int i = 0; i < 10; ++i) {
        longPath += "/segment" + std::to_string(i);
    }
    router.insert(Gecko::HttpMethod::GET, longPath, wrap_response_handler(handlerUsers));
    auto result4 = router.find(Gecko::HttpMethod::GET, longPath);
    assert(result4.has_value());
    
    /* Path with special characters */
    router.insert(Gecko::HttpMethod::GET, "/api/users/:user-id/posts", wrap_response_handler(handlerUserPosts));
    auto result5 = router.find(Gecko::HttpMethod::GET, "/api/users/user123/posts");
    assert(result5.has_value());
    assert(result5->params["user-id"] == "user123");
    
}

void test_handler_execution() {
    Gecko::Router router;
    
    /* Register routes */
    router.insert(Gecko::HttpMethod::GET, "/test", wrap_response_handler(handlerHome));
    router.insert(Gecko::HttpMethod::GET, "/users/:id", wrap_response_handler(handlerUserDetail));
    
    /* Build mock request */
    Gecko::HttpRequest request;
    request.setMethod(Gecko::HttpMethod::GET);
    request.setUrl("/test");
    
    /* Execute handler */
    auto result1 = router.find(Gecko::HttpMethod::GET, "/test");
    assert(result1.has_value());
    
    if (result1->handler) {
        Gecko::Context ctx(request);
        result1->handler(ctx);
        auto response = ctx.response();
        assert(response.getStatusCode() == 200);
        assert(response.getBody() == "Home Page");
    }
    
    /* Execute parameterized handler */
    auto result2 = router.find(Gecko::HttpMethod::GET, "/users/123");
    assert(result2.has_value());
    assert(result2->params["id"] == "123");
    
    if (result2->handler) {
        Gecko::Context ctx(request);
        ctx.setParams(result2->params);
        result2->handler(ctx);
        auto response = ctx.response();
        assert(response.getStatusCode() == 200);
        assert(response.getBody() == "User Detail");
    }
    
}

int main() {
    test_split_path();
    test_static_routes();
    test_param_routes();
    test_mixed_routes();
    test_multiple_params();
    test_route_conflicts();
    test_edge_cases();
    test_handler_execution();
    
    std::cout << "all tests passed" << std::endl;
    return 0;
} 
