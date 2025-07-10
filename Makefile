CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -pthread
INCLUDES = -I.

# 核心源文件（排除测试文件）
SRC_DIR = src
CORE_SOURCES = $(SRC_DIR)/http_request.cpp $(SRC_DIR)/http_response.cpp $(SRC_DIR)/router.cpp $(SRC_DIR)/server.cpp $(SRC_DIR)/thread_pool.cpp $(SRC_DIR)/logger.cpp
CORE_OBJECTS = $(CORE_SOURCES:.cpp=.o)

# 测试程序
TEST_PROGRAMS = example_gin_style

# 默认目标
all: $(TEST_PROGRAMS)

# 编译对象文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 编译Gin风格示例（更新了配置API）
example_gin_style: example_gin_style.cpp $(CORE_OBJECTS) src/context.cpp src/engine.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ -pthread

# 清理
clean:
	rm -f $(CORE_OBJECTS) $(TEST_PROGRAMS) *.o
	rm -f src/test_*.o  # 清理测试文件的对象文件

# 显示帮助
help:
	@echo "🦎 Gecko Web Framework 构建系统"
	@echo "==============================="
	@echo ""
	@echo "可用目标："
	@echo "  all         - 编译所有程序"
	@echo "  clean       - 清理编译文件"
	@echo "  help        - 显示帮助"
	@echo ""
	@echo "核心源文件："
	@echo "  $(CORE_SOURCES)"
	@echo ""
	@echo "测试步骤："
	@echo "  1. make clean"
	@echo "  2. make"
	@echo "  3. ./example_gin_style"
	@echo "  4. 在浏览器访问 http://localhost:13514"
	@echo "  5. 测试各个端点，观察多线程处理"
	@echo "  6. 按 Ctrl+C 停止服务器"
	@echo ""
	@echo "最新特性："
	@echo "  ✅ 多线程请求处理（使用系统最大线程数）"
	@echo "  ✅ 配置化服务器启动"
	@echo "  ✅ 灵活的日志系统设计"
	@echo "  ✅ 线程池优化"
	@echo "  ✅ 响应中包含线程ID信息"

.PHONY: all clean help 