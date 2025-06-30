#include <iostream>
#include <cassert>
#include "router.hpp"

// 模拟的请求处理函数
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

void test_split_path() {
    std::cout << "=== 测试split_path工具函数 ===" << std::endl;
    
    // 测试根路径
    auto result1 = Gecko::split_path("/");
    assert(result1.empty());
    
    // 测试简单路径
    auto result2 = Gecko::split_path("/users");
    assert(result2.size() == 1);
    assert(result2[0] == "users");
    
    // 测试复杂路径
    auto result3 = Gecko::split_path("/users/:id/posts");
    assert(result3.size() == 3);
    assert(result3[0] == "users");
    assert(result3[1] == ":id");
    assert(result3[2] == "posts");
    
    // 测试带多个斜杠的路径
    auto result4 = Gecko::split_path("///users//posts///");
    assert(result4.size() == 2);
    assert(result4[0] == "users");
    assert(result4[1] == "posts");
    
    std::cout << "✓ split_path测试通过" << std::endl;
}

void test_static_routes() {
    std::cout << "=== 测试静态路由 ===" << std::endl;
    
    Gecko::Router router;
    
    // 插入静态路由
    router.insert(Gecko::HttpMethod::GET, "/", handlerHome);
    router.insert(Gecko::HttpMethod::GET, "/users", handlerUsers);
    router.insert(Gecko::HttpMethod::POST, "/users", handlerCreateUser);
    router.insert(Gecko::HttpMethod::GET, "/users/profile", handlerUserDetail);
    
    // 测试GET /
    auto result1 = router.find(Gecko::HttpMethod::GET, "/");
    assert(result1.has_value());
    assert(result1->handler != nullptr);
    assert(result1->params.empty());
    
    // 测试GET /users
    auto result2 = router.find(Gecko::HttpMethod::GET, "/users");
    assert(result2.has_value());
    assert(result2->handler != nullptr);
    assert(result2->params.empty());
    
    // 测试POST /users（不同HTTP方法）
    auto result3 = router.find(Gecko::HttpMethod::POST, "/users");
    assert(result3.has_value());
    assert(result3->handler != nullptr);
    assert(result3->params.empty());
    
    // 测试嵌套路径
    auto result4 = router.find(Gecko::HttpMethod::GET, "/users/profile");
    assert(result4.has_value());
    assert(result4->handler != nullptr);
    assert(result4->params.empty());
    
    // 测试不存在的路由
    auto result5 = router.find(Gecko::HttpMethod::GET, "/nonexistent");
    assert(!result5.has_value());
    
    // 测试不存在的HTTP方法
    auto result6 = router.find(Gecko::HttpMethod::DELETE, "/users");
    assert(!result6.has_value());
    
    std::cout << "✓ 静态路由测试通过" << std::endl;
}

void test_param_routes() {
    std::cout << "=== 测试参数路由 ===" << std::endl;
    
    Gecko::Router router;
    
    // 插入参数路由
    router.insert(Gecko::HttpMethod::GET, "/users/:id", handlerUserDetail);
    router.insert(Gecko::HttpMethod::PUT, "/users/:id", handlerUpdateUser);
    router.insert(Gecko::HttpMethod::GET, "/users/:id/posts", handlerUserPosts);
    
    // 测试单个参数
    auto result1 = router.find(Gecko::HttpMethod::GET, "/users/123");
    assert(result1.has_value());
    assert(result1->handler != nullptr);
    assert(result1->params.size() == 1);
    assert(result1->params["id"] == "123");
    
    // 测试不同参数值
    auto result2 = router.find(Gecko::HttpMethod::GET, "/users/abc");
    assert(result2.has_value());
    assert(result2->params["id"] == "abc");
    
    // 测试不同HTTP方法，相同路径
    auto result3 = router.find(Gecko::HttpMethod::PUT, "/users/456");
    assert(result3.has_value());
    assert(result3->params["id"] == "456");
    
    // 测试嵌套参数路由
    auto result4 = router.find(Gecko::HttpMethod::GET, "/users/789/posts");
    assert(result4.has_value());
    assert(result4->params["id"] == "789");
    
    // 测试参数路由不匹配
    auto result5 = router.find(Gecko::HttpMethod::GET, "/users");
    assert(!result5.has_value());
    
    auto result6 = router.find(Gecko::HttpMethod::GET, "/users/123/comments");
    assert(!result6.has_value());
    
    std::cout << "✓ 参数路由测试通过" << std::endl;
}

void test_mixed_routes() {
    std::cout << "=== 测试混合路由 ===" << std::endl;
    
    Gecko::Router router;
    
    // 插入混合路由（静态和参数混合）
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users", handlerUsers);
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users/:id", handlerUserDetail);
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users/:id/posts", handlerUserPosts);
    router.insert(Gecko::HttpMethod::GET, "/api/v1/users/profile", handlerUserDetail);
    
    // 测试静态路由优先级（应该匹配具体的静态路由而不是参数路由）
    auto result1 = router.find(Gecko::HttpMethod::GET, "/api/v1/users/profile");
    assert(result1.has_value());
    assert(result1->params.empty());  // 应该是静态路由，没有参数
    
    // 测试参数路由
    auto result2 = router.find(Gecko::HttpMethod::GET, "/api/v1/users/123");
    assert(result2.has_value());
    assert(result2->params["id"] == "123");
    
    // 测试嵌套混合路由
    auto result3 = router.find(Gecko::HttpMethod::GET, "/api/v1/users/456/posts");
    assert(result3.has_value());
    assert(result3->params["id"] == "456");
    
    // 测试列表路由
    auto result4 = router.find(Gecko::HttpMethod::GET, "/api/v1/users");
    assert(result4.has_value());
    assert(result4->params.empty());
    
    std::cout << "✓ 混合路由测试通过" << std::endl;
}

void test_multiple_params() {
    std::cout << "=== 测试多参数路由 ===" << std::endl;
    
    Gecko::Router router;
    
    // 插入多参数路由
    router.insert(Gecko::HttpMethod::GET, "/users/:userId/posts/:postId", 
                  [](const Gecko::HttpRequest &req) -> Gecko::HttpResponse {
                      auto response = Gecko::HttpResponse::stockResponse(200);
                      response.setBody("User Post Detail");
                      return response;
                  });
    
    router.insert(Gecko::HttpMethod::GET, "/api/:version/users/:id/settings/:key",
                  [](const Gecko::HttpRequest &req) -> Gecko::HttpResponse {
                      auto response = Gecko::HttpResponse::stockResponse(200);
                      response.setBody("User Setting");
                      return response;
                  });
    
    // 测试双参数路由
    auto result1 = router.find(Gecko::HttpMethod::GET, "/users/123/posts/456");
    assert(result1.has_value());
    assert(result1->params.size() == 2);
    assert(result1->params["userId"] == "123");
    assert(result1->params["postId"] == "456");
    
    // 测试多参数路由
    auto result2 = router.find(Gecko::HttpMethod::GET, "/api/v2/users/789/settings/theme");
    assert(result2.has_value());
    assert(result2->params.size() == 3);
    assert(result2->params["version"] == "v2");
    assert(result2->params["id"] == "789");
    assert(result2->params["key"] == "theme");
    
    // 测试参数不完整的情况
    auto result3 = router.find(Gecko::HttpMethod::GET, "/users/123/posts");
    assert(!result3.has_value());
    
    std::cout << "✓ 多参数路由测试通过" << std::endl;
}

void test_route_conflicts() {
    std::cout << "=== 测试路由冲突处理 ===" << std::endl;
    
    Gecko::Router router;
    
    // 测试路由覆盖（同一路径，同一HTTP方法）
    router.insert(Gecko::HttpMethod::GET, "/users/:id", handlerUserDetail);
    router.insert(Gecko::HttpMethod::GET, "/users/:id", handlerUsers);  // 覆盖前一个
    
    auto result1 = router.find(Gecko::HttpMethod::GET, "/users/123");
    assert(result1.has_value());
    // 应该是最后插入的handler
    
    // 测试不同HTTP方法不冲突
    router.insert(Gecko::HttpMethod::POST, "/users/:id", handlerCreateUser);
    router.insert(Gecko::HttpMethod::PUT, "/users/:id", handlerUpdateUser);
    
    auto result2 = router.find(Gecko::HttpMethod::POST, "/users/123");
    assert(result2.has_value());
    
    auto result3 = router.find(Gecko::HttpMethod::PUT, "/users/123");
    assert(result3.has_value());
    
    std::cout << "✓ 路由冲突处理测试通过" << std::endl;
}

void test_edge_cases() {
    std::cout << "=== 测试边缘情况 ===" << std::endl;
    
    Gecko::Router router;
    
    // 测试空路径
    router.insert(Gecko::HttpMethod::GET, "", handlerHome);
    auto result1 = router.find(Gecko::HttpMethod::GET, "");
    assert(result1.has_value());
    
    // 测试只有斜杠的路径
    router.insert(Gecko::HttpMethod::GET, "/", handlerHome);
    auto result2 = router.find(Gecko::HttpMethod::GET, "/");
    assert(result2.has_value());
    
    // 测试参数名称为空的情况（虽然不推荐）
    router.insert(Gecko::HttpMethod::GET, "/test/:", handlerUserDetail);
    auto result3 = router.find(Gecko::HttpMethod::GET, "/test/value");
    assert(result3.has_value());
    assert(result3->params[""] == "value");  // 空参数名
    
    // 测试很长的路径
    std::string longPath = "/api";
    for (int i = 0; i < 10; ++i) {
        longPath += "/segment" + std::to_string(i);
    }
    router.insert(Gecko::HttpMethod::GET, longPath, handlerUsers);
    auto result4 = router.find(Gecko::HttpMethod::GET, longPath);
    assert(result4.has_value());
    
    // 测试包含特殊字符的路径段
    router.insert(Gecko::HttpMethod::GET, "/api/users/:user-id/posts", handlerUserPosts);
    auto result5 = router.find(Gecko::HttpMethod::GET, "/api/users/user123/posts");
    assert(result5.has_value());
    assert(result5->params["user-id"] == "user123");
    
    std::cout << "✓ 边缘情况测试通过" << std::endl;
}

void test_handler_execution() {
    std::cout << "=== 测试Handler执行 ===" << std::endl;
    
    Gecko::Router router;
    
    // 插入路由
    router.insert(Gecko::HttpMethod::GET, "/test", handlerHome);
    router.insert(Gecko::HttpMethod::GET, "/users/:id", handlerUserDetail);
    
    // 创建模拟请求
    Gecko::HttpRequest request;
    request.setMethod(Gecko::HttpMethod::GET);
    request.setUrl("/test");
    
    // 测试handler执行
    auto result1 = router.find(Gecko::HttpMethod::GET, "/test");
    assert(result1.has_value());
    
    if (result1->handler) {
        auto response = result1->handler(request);
        assert(response.getStatusCode() == 200);
        assert(response.getBody() == "Home Page");
    }
    
    // 测试参数路由的handler执行
    auto result2 = router.find(Gecko::HttpMethod::GET, "/users/123");
    assert(result2.has_value());
    assert(result2->params["id"] == "123");
    
    if (result2->handler) {
        auto response = result2->handler(request);
        assert(response.getStatusCode() == 200);
        assert(response.getBody() == "User Detail");
    }
    
    std::cout << "✓ Handler执行测试通过" << std::endl;
}

int main() {
    std::cout << "开始Router路由器测试..." << std::endl << std::endl;
    
    test_split_path();
    test_static_routes();
    test_param_routes();
    test_mixed_routes();
    test_multiple_params();
    test_route_conflicts();
    test_edge_cases();
    test_handler_execution();
    
    std::cout << std::endl << "🎉 所有Router测试都通过了！" << std::endl;
    std::cout << std::endl << "路由器功能验证：" << std::endl;
    std::cout << "✓ 静态路由匹配" << std::endl;
    std::cout << "✓ 参数路由匹配和参数提取" << std::endl;
    std::cout << "✓ HTTP方法区分" << std::endl;
    std::cout << "✓ 多级嵌套路由" << std::endl;
    std::cout << "✓ 多参数路由" << std::endl;
    std::cout << "✓ 路由冲突处理" << std::endl;
    std::cout << "✓ 边缘情况处理" << std::endl;
    std::cout << "✓ Handler正确执行" << std::endl;
    std::cout << std::endl << "Gecko路由器工作正常！" << std::endl;
    
    return 0;
} 