CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -pthread
INCLUDES = -I.

# 核心源文件（排除测试文件）
SRC_DIR = src
CORE_SOURCES = $(SRC_DIR)/http_request.cpp $(SRC_DIR)/http_response.cpp $(SRC_DIR)/router.cpp $(SRC_DIR)/server.cpp $(SRC_DIR)/thread_pool.cpp
CORE_OBJECTS = $(CORE_SOURCES:.cpp=.o)

# 测试程序
TEST_PROGRAMS = example_gin_style example_multithread

# 默认目标
all: $(TEST_PROGRAMS)

# 编译对象文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 编译Gin风格示例
example_gin_style: example_gin_style.cpp $(CORE_OBJECTS) src/context.cpp src/engine.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ -pthread

# 编译多线程示例
example_multithread: example_multithread.cpp $(CORE_OBJECTS) src/context.cpp src/engine.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ -pthread


# 清理
clean:
	rm -f $(CORE_OBJECTS) $(TEST_PROGRAMS) *.o
	rm -f src/test_*.o  # 清理测试文件的对象文件

# 显示帮助
help:
	@echo "🦎 Gecko Web Framework 测试系统"
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
	@echo "  2. make test"
	@echo "  3. 在浏览器访问 http://localhost:8080"
	@echo "  4. 点击测试链接验证功能"
	@echo "  5. 按 Ctrl+C 停止服务器"
	@echo ""
	@echo "新特性："
	@echo "  ✅ 多线程请求处理"
	@echo "  ✅ 线程池优化"
	@echo "  ✅ 异步请求处理"

.PHONY: all clean help test 