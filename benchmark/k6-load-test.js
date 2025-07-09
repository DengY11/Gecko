import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';

// 自定义指标
const errorRate = new Rate('errors');
const apiResponseTimes = new Trend('api_response_times');
const requestCounter = new Counter('requests_total');

// 服务器配置
const BASE_URL = 'http://localhost:13514';

// 测试配置
export const options = {
  // 分阶段负载测试
  stages: [
    { duration: '30s', target: 20 },   // 预热: 30秒内增加到20个用户
    { duration: '1m', target: 50 },    // 增压: 1分钟内增加到50个用户
    { duration: '2m', target: 100 },   // 高压: 2分钟内增加到100个用户
    { duration: '1m', target: 200 },   // 峰值: 1分钟内增加到200个用户
    { duration: '30s', target: 100 },  // 降压: 30秒内降到100个用户
    { duration: '30s', target: 0 },    // 结束: 30秒内降到0个用户
  ],
  
  // 阈值定义
  thresholds: {
    http_req_duration: ['p(95)<500'], // 95%的请求延迟应该小于500ms
    http_req_failed: ['rate<0.01'],   // 错误率应该小于1%
    errors: ['rate<0.05'],            // 自定义错误率应该小于5%
    api_response_times: ['p(99)<1000'], // 99%的API响应时间应该小于1000ms
  },
  
  // 全局配置
  userAgent: 'K6-LoadTester/1.0',
  noConnectionReuse: false,
  httpDebug: 'full',
};

// 测试数据
const testUsers = ['alice', 'bob', 'charlie', 'diana', 'eve'];
const searchQueries = ['gecko', 'framework', 'performance', 'benchmark', 'test'];

export default function() {
  const scenarios = [
    testHomePage,
    testPingAPI,
    testUserAPI,
    testSearchAPI,
    testHeadersAPI,
    testCreateUser
  ];
  
  // 随机选择一个测试场景
  const scenario = scenarios[Math.floor(Math.random() * scenarios.length)];
  scenario();
  
  // 短暂等待
  sleep(Math.random() * 2 + 0.5); // 0.5-2.5秒随机等待
}

// 测试首页
function testHomePage() {
  const response = http.get(`${BASE_URL}/`);
  
  const success = check(response, {
    '首页状态码为200': (r) => r.status === 200,
    '首页包含Gecko': (r) => r.body.includes('Gecko'),
    '首页响应时间<200ms': (r) => r.timings.duration < 200,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试Ping API
function testPingAPI() {
  const response = http.get(`${BASE_URL}/ping`);
  
  const success = check(response, {
    'Ping状态码为200': (r) => r.status === 200,
    'Ping返回JSON': (r) => r.headers['Content-Type'].includes('application/json'),
    'Ping包含message字段': (r) => {
      try {
        const json = JSON.parse(r.body);
        return json.message === 'pong';
      } catch (e) {
        return false;
      }
    },
    'Ping响应时间<100ms': (r) => r.timings.duration < 100,
  });
  
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
}

// 测试用户API
function testUserAPI() {
  const userId = Math.random() > 0.5 ? '123' : '456';
  const response = http.get(`${BASE_URL}/api/users/${userId}`);
  
  const success = check(response, {
    '用户API状态码为200': (r) => r.status === 200,
    '用户API返回JSON': (r) => r.headers['Content-Type'].includes('application/json'),
    '用户API包含id字段': (r) => {
      try {
        const json = JSON.parse(r.body);
        return json.id !== undefined;
      } catch (e) {
        return false;
      }
    },
    '用户API响应时间<150ms': (r) => r.timings.duration < 150,
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
    '搜索API返回JSON': (r) => r.headers['Content-Type'].includes('application/json'),
    '搜索API包含查询字段': (r) => {
      try {
        const json = JSON.parse(r.body);
        return json.search_query === query;
      } catch (e) {
        return false;
      }
    },
    '搜索API响应时间<200ms': (r) => r.timings.duration < 200,
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
    '请求头API返回JSON': (r) => r.headers['Content-Type'].includes('application/json'),
    '请求头API包含headers字段': (r) => {
      try {
        const json = JSON.parse(r.body);
        return json.headers !== undefined;
      } catch (e) {
        return false;
      }
    },
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
    '创建用户返回JSON': (r) => r.headers['Content-Type'].includes('application/json'),
    '创建用户响应时间<300ms': (r) => r.timings.duration < 300,
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
    'Hello API返回JSON': (r) => r.headers['Content-Type'].includes('application/json'),
    'Hello API包含正确名称': (r) => {
      try {
        const json = JSON.parse(r.body);
        return json.message.includes(name);
      } catch (e) {
        return false;
      }
    },
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