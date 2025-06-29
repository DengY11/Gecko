#!/bin/bash

echo "=== Gecko HTTP Request 测试套件 ==="
echo

# 编译器设置
CXX="g++"
CXXFLAGS="-std=c++23 -Wall -Wextra -O2"
SRC_DIR="src"

# 测试文件列表
tests=(
    "test_basic_requests"
    "test_error_cases"
    "test_constructors"
    "test_special_cases"
)

# 编译和运行每个测试
for test in "${tests[@]}"; do
    echo "=== 编译 ${test} ==="
    if ${CXX} ${CXXFLAGS} ${SRC_DIR}/${test}.cpp ${SRC_DIR}/http_request.cpp -o ${test}_runner; then
        echo "✓ ${test} 编译成功"
        echo
        echo "=== 运行 ${test} ==="
        if ./${test}_runner; then
            echo "✓ ${test} 运行成功"
        else
            echo "✗ ${test} 运行失败"
            exit 1
        fi
        echo
        # 清理可执行文件
        rm -f ${test}_runner
    else
        echo "✗ ${test} 编译失败"
        exit 1
    fi
    echo "----------------------------------------"
done

echo "🎉 所有测试都通过了！"
echo
echo "测试统计："
echo "- 基本HTTP请求测试 (GET, POST, PUT, DELETE, HEAD)"
echo "- 错误处理和边界情况测试"
echo "- 构造函数和赋值操作符测试"
echo "- 特殊情况测试 (空格处理、长URL、特殊字符等)"
echo
echo "Gecko HTTP请求解析器工作正常！" 
