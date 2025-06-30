CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
INCLUDES = -I.

# 核心源文件（排除测试文件）
SRC_DIR = src
CORE_SOURCES = $(SRC_DIR)/http_request.cpp $(SRC_DIR)/http_response.cpp $(SRC_DIR)/router.cpp $(SRC_DIR)/server.cpp
CORE_OBJECTS = $(CORE_SOURCES:.cpp=.o)

# 测试程序
TEST_PROGRAMS = test_server

# 默认目标
all: $(TEST_PROGRAMS)

# 编译对象文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 编译Server测试（只链接核心对象文件）
test_server: test_server.cpp $(CORE_OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

# 运行Server测试
test: test_server
	@echo "🦎 启动 Gecko Web Framework Server 测试..."
	@echo "在浏览器中访问 http://localhost:8080 进行测试"
	@echo "按 Ctrl+C 停止测试"
	./test_server

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
	@echo "  test_server - 编译Server测试程序"
	@echo "  test        - 运行Server测试"
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

.PHONY: all clean help test 