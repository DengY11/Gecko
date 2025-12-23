#!/bin/bash

echo "=== Gecko Web Framework 系统优化脚本 ==="

# Check privilege level
if [ "$EUID" -ne 0 ]; then
    echo "注意: 某些优化需要root权限才能永久生效"
    echo "建议运行: sudo ./optimize_system.sh"
    echo ""
fi

echo "1. 优化网络参数..."

# Expand ephemeral port range (root only)
if [ "$EUID" -eq 0 ]; then
    echo "net.ipv4.ip_local_port_range = 1024 65535" >> /etc/sysctl.conf
    sysctl -w net.ipv4.ip_local_port_range="1024 65535"
    echo "[OK] 端口范围扩展至 1024-65535 (64511个端口)"
else
    echo "- 当前端口范围: $(cat /proc/sys/net/ipv4/ip_local_port_range)"
    echo "  建议扩展至: 1024 65535 (需要root权限)"
fi

# Accelerate TIME_WAIT recycling (root only)
if [ "$EUID" -eq 0 ]; then
    echo "net.ipv4.tcp_tw_reuse = 1" >> /etc/sysctl.conf
    echo "net.ipv4.tcp_fin_timeout = 30" >> /etc/sysctl.conf
    sysctl -w net.ipv4.tcp_tw_reuse=1
    sysctl -w net.ipv4.tcp_fin_timeout=30
    echo "[OK] 启用TIME_WAIT重用，减少FIN_TIMEOUT为30秒"
else
    echo "- TIME_WAIT重用: $(cat /proc/sys/net/ipv4/tcp_tw_reuse)"
    echo "  建议启用TIME_WAIT重用以加速端口回收"
fi

# Increase listen queue size (root only)
if [ "$EUID" -eq 0 ]; then
    echo "net.core.somaxconn = 65536" >> /etc/sysctl.conf
    echo "net.ipv4.tcp_max_syn_backlog = 65536" >> /etc/sysctl.conf
    sysctl -w net.core.somaxconn=65536
    sysctl -w net.ipv4.tcp_max_syn_backlog=65536
    echo "[OK] 连接队列扩展至 65536"
else
    echo "- 当前somaxconn: $(cat /proc/sys/net/core/somaxconn)"
    echo "  建议增加至65536以支持更多并发连接"
fi

echo ""
echo "2. 文件描述符优化..."

# Inspect descriptor limits
current_limit=$(ulimit -n)
echo "- 当前进程文件描述符限制: $current_limit"

if [ $current_limit -lt 1000000 ]; then
    echo "- 建议增加到1,000,000: ulimit -n 1000000"
    ulimit -n 1000000 2>/dev/null || echo "  (需要修改 /etc/security/limits.conf)"
fi

echo ""
echo "3. 合理的测试配置建议..."
echo ""
echo "基于您的系统，建议的K6测试配置："
echo "- 最大VU数: 30,000 (低于可用端口数)"
echo "- 测试持续时间: 较短 (3-5分钟)"
echo "- 渐进增长: 逐步增加VU数，观察系统表现"
echo ""

# Derive available ports
port_range=$(cat /proc/sys/net/ipv4/ip_local_port_range)
min_port=$(echo $port_range | cut -d' ' -f1)
max_port=$(echo $port_range | cut -d' ' -f2)
available_ports=$((max_port - min_port + 1))

echo "4. 当前系统能力评估..."
echo "- 可用客户端端口: $available_ports"
echo "- TIME_WAIT连接数: $(ss -s | grep timewait | grep -o '[0-9]*' | head -1)"
echo "- 建议最大并发VU: $((available_ports * 80 / 100)) (预留20%安全边界)"

echo ""
echo "=== 优化完成 ==="

if [ "$EUID" -eq 0 ]; then
    echo "系统参数已永久修改，重启后生效"
    echo "立即生效: sysctl -p"
else
    echo "要永久生效，请以root权限运行此脚本"
fi 
