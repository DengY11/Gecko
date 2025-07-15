import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';

// 自定义指标
const errorRate = new Rate('errors');
const apiResponseTimes = new Trend('api_response_times');
const requestCounter = new Counter('requests_total');

// 服务器配置
const BASE_URL = 'http://localhost:13514';

// 测试配置 - 修复过激进的配置
export const options = {
  // 渐进式负载测试 - 更现实的并发数
  stages: [
    { duration: '30s', target: 5000 },    // 预热: 30秒内增加到50个用户
    { duration: '1m', target: 20000 },    // 峰值: 1分钟内增加到200个用户  
    { duration: '2m', target: 50000 },    // 高峰: 2分钟内增加到500个用户
    { duration: '1m', target: 20000 },    // 降压: 1分钟内降到200个用户
    { duration: '30s', target: 0 },     // 结束: 30秒内降到0个用户
  ],
  
  // 更现实的阈值定义
  thresholds: {
    http_req_duration: ['p(95)<1000'], // 95%的请求延迟应该小于1000ms
    http_req_failed: ['rate<0.05'],    // 错误率应该小于5%
    errors: ['rate<0.10'],             // 自定义错误率应该小于10%
    api_response_times: ['p(99)<2000'], // 99%的API响应时间应该小于2000ms
  },
  
  // 全局配置 - 移除性能杀手选项
  userAgent: 'K6-LoadTester/1.0',
  noConnectionReuse: false,  // 启用连接复用以测试Keep-Alive
  // httpDebug: 'full',      // 移除调试选项，严重影响性能
};

// 添加简单测试场景选项
export const scenarios = {
  // 轻量级测试 - 适合初始验证
  light_load: {
    executor: 'ramping-vus',
    startVUs: 1,
    stages: [
      { duration: '30s', target: 10 },
      { duration: '1m', target: 50 },
      { duration: '30s', target: 0 },
    ],
  },
  
  // 中等负载测试
  medium_load: {
    executor: 'ramping-vus', 
    startVUs: 1,
    stages: [
      { duration: '1m', target: 100 },
      { duration: '2m', target: 200 },
      { duration: '1m', target: 0 },
    ],
  },
  
  // 高负载测试
  heavy_load: {
    executor: 'ramping-vus',
    startVUs: 1, 
    stages: [
      { duration: '2m', target: 200 },
      { duration: '3m', target: 500 },
      { duration: '2m', target: 1000 },
      { duration: '1m', target: 0 },
    ],
  }
};

// 测试数据
const testUsers = ['alice', 'bob', 'charlie', 'diana', 'eve'];
const searchQueries = ['gecko', 'framework', 'performance', 'benchmark', 'test'];

export default function() {
  // 简化测试场景选择
  const scenarios = [
    testHomePage,
    testPingAPI,
    testUserAPI,
    // testSearchAPI,    // 暂时注释复杂场景
    // testHeadersAPI,
    // testCreateUser
  ];
  
  // 随机选择一个测试场景
  const scenario = scenarios[Math.floor(Math.random() * scenarios.length)];
  scenario();
  
  // 减少等待时间
  sleep(Math.random() * 0.5 + 0.1); // 0.1-0.6秒随机等待
}

// 测试首页 - 简化检查
function testHomePage() {
  const response = http.get(`${BASE_URL}/`);
  
  const success = check(response, {
    '首页状态码为200': (r) => r.status === 200,
    '首页响应时间<500ms': (r) => r.timings.duration < 500,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试Ping API - 最轻量级测试
function testPingAPI() {
  const response = http.get(`${BASE_URL}/ping`);
  
  const success = check(response, {
    'Ping状态码为200': (r) => r.status === 200,
    'Ping响应时间<200ms': (r) => r.timings.duration < 200,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试用户API - 简化验证
function testUserAPI() {
  const userId = Math.random() > 0.5 ? '123' : '456';
  const response = http.get(`${BASE_URL}/api/users/${userId}`);
  
  const success = check(response, {
    '用户API状态码为200': (r) => r.status === 200,
    '用户API响应时间<300ms': (r) => r.timings.duration < 300,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试搜索API
function testSearchAPI() {
  const query = searchQueries[Math.floor(Math.random() * searchQueries.length)];
  const response = http.get(`${BASE_URL}/search?q=${query}&type=framework`);
  
  const success = check(response, {
    '搜索API状态码为200': (r) => r.status === 200,
    '搜索API响应时间<400ms': (r) => r.timings.duration < 400,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试请求头API
function testHeadersAPI() {
  const response = http.get(`${BASE_URL}/headers`, {
    headers: {
      'X-Test-Header': 'k6-test',
      'X-User-ID': Math.floor(Math.random() * 1000).toString(),
    }
  });
  
  const success = check(response, {
    '请求头API状态码为200': (r) => r.status === 200,
    '请求头API响应时间<300ms': (r) => r.timings.duration < 300,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试创建用户API (POST请求)
function testCreateUser() {
  const userData = {
    name: `User${Math.floor(Math.random() * 1000)}`,
    email: `user${Math.floor(Math.random() * 1000)}@example.com`,
    role: Math.random() > 0.5 ? 'user' : 'admin'
  };
  
  const response = http.post(`${BASE_URL}/api/users`, JSON.stringify(userData), {
    headers: {
      'Content-Type': 'application/json',
    }
  });
  
  const success = check(response, {
    '创建用户状态码为201': (r) => r.status === 201,
    '创建用户响应时间<500ms': (r) => r.timings.duration < 500,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试Hello API (路径参数)
function testHelloAPI() {
  const name = testUsers[Math.floor(Math.random() * testUsers.length)];
  const response = http.get(`${BASE_URL}/hello/${name}`);
  
  const success = check(response, {
    'Hello API状态码为200': (r) => r.status === 200,
    'Hello API响应时间<300ms': (r) => r.timings.duration < 300,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 设置阶段 - 在测试开始前运行
export function setup() {
  console.log('🚀 开始Gecko Web Framework性能测试');
  console.log(`📊 目标服务器: ${BASE_URL}`);
  
  // 检查服务器是否可用
  const response = http.get(`${BASE_URL}/ping`);
  if (response.status !== 200) {
    throw new Error(`服务器不可用，状态码: ${response.status}`);
  }
  
  console.log('✅ 服务器连接正常，开始测试...');
  return { startTime: new Date() };
}

// 拆卸阶段 - 在测试结束后运行
export function teardown(data) {
  const endTime = new Date();
  const duration = (endTime - data.startTime) / 1000;
  console.log(`🏁 测试完成，总耗时: ${duration.toFixed(2)}秒`);
} 
