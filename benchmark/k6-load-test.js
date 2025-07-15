import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';

// 自定义指标
const errorRate = new Rate('errors');
const apiResponseTimes = new Trend('api_response_times');
const requestCounter = new Counter('requests_total');
const connectionsActive = new Counter('connections_active');

// 服务器配置
const BASE_URL = 'http://localhost:13514';

// 测试配置 - 专注于连接持续性
export const options = {
  // 持续连接测试 - 最大20000个连接
  stages: [
    { duration: '2m', target: 5000 },     // 2分钟内增加到5000个连接
    { duration: '2m', target: 20000 },    // 2分钟内增加到20000个连接（峰值）
     {duration: '2m', target: 10000 },    // 2分钟内降到10000个连接
    { duration: '1m', target: 0 },        // 1分钟内所有连接断开
  ],
  
  // 连接持续性相关的阈值
  thresholds: {
    http_req_duration: ['p(95)<2000'],    // 95%的请求延迟应该小于2秒
    http_req_failed: ['rate<0.02'],       // 错误率应该小于2%
    errors: ['rate<0.05'],                // 自定义错误率应该小于5%
    api_response_times: ['p(99)<5000'],   // 99%的API响应时间应该小于5秒
    requests_total: ['count>100000'],     // 总请求数应该超过10万
  },
  
  // 连接复用配置 - 模拟真实浏览器行为
  userAgent: 'K6-Gecko-LoadTester/1.0',
  noConnectionReuse: false,              // 启用连接复用测试Keep-Alive
  maxRedirects: 4,
  batch: 1,                              // 每次只发送一个请求，模拟真实场景
  // httpDebug: 'full',                  // 关闭调试输出，提高性能
};

// 简化场景配置 - 专注于持续连接
export const scenarios = {
  // 默认场景：持续连接持续请求
  continuous_load: {
    executor: 'ramping-vus',
    startVUs: 0,
    stages: [
      { duration: '2m', target: 5000 },
      { duration: '3m', target: 10000 },
      { duration: '3m', target: 15000 },
      { duration: '2m', target: 20000 },
      { duration: '10m', target: 20000 },  // 核心：保持20000个连接10分钟
      { duration: '4m', target: 0 },
    ],
    gracefulRampDown: '1m',               // 优雅关闭时间
  }
};

// 测试数据
const testUsers = ['alice', 'bob', 'charlie', 'diana', 'eve', 'frank', 'grace', 'henry'];
const searchQueries = ['gecko', 'framework', 'performance', 'benchmark', 'test', 'api', 'web', 'server'];

export default function() {
  // 每个虚拟用户（连接）会持续发送多个请求
  const requestsPerConnection = Math.floor(Math.random() * 5) + 3; // 每个连接发送3-7个请求
  
  for (let i = 0; i < requestsPerConnection; i++) {
    // 随机选择测试场景，模拟真实用户行为
    const testScenarios = [
      testPingAPI,       // 40% - 最轻量级
      testHomePage,      // 25% - 常见请求
      testUserAPI,       // 20% - 业务请求
      testHelloAPI,      // 10% - 参数化请求
      testSearchAPI,     // 5%  - 复杂请求
    ];
    
    const weights = [40, 25, 20, 10, 5];
    const randomValue = Math.random() * 100;
    let cumulativeWeight = 0;
    
    for (let j = 0; j < testScenarios.length; j++) {
      cumulativeWeight += weights[j];
      if (randomValue <= cumulativeWeight) {
        testScenarios[j]();
        break;
      }
    }
    
    // 模拟用户在请求之间的思考时间
    sleep(Math.random() * 0.3 + 0.1); // 0.1-0.4秒的间隔
  }
  
  // 连接结束前的长暂停，模拟用户阅读内容的时间
  sleep(Math.random() * 2 + 1); // 1-3秒的暂停
}

// 测试Ping API - 最轻量级，用于保活检测
function testPingAPI() {
  const response = http.get(`${BASE_URL}/ping`, {
    headers: {
      'Connection': 'keep-alive',
      'Cache-Control': 'no-cache',
    }
  });
  
  const success = check(response, {
    'Ping状态码为200': (r) => r.status === 200,
    'Ping响应时间<1000ms': (r) => r.timings.duration < 1000,
    'Ping响应包含pong': (r) => r.body && r.body.includes('pong'),
  });
  
  updateMetrics(success, response);
}

// 测试首页
function testHomePage() {
  const response = http.get(`${BASE_URL}/`, {
    headers: {
      'Connection': 'keep-alive',
      'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
    }
  });
  
  const success = check(response, {
    '首页状态码为200': (r) => r.status === 200,
    '首页响应时间<2000ms': (r) => r.timings.duration < 2000,
    '首页包含Gecko': (r) => r.body && r.body.includes('Gecko'),
  });
  
  updateMetrics(success, response);
}

// 测试用户API
function testUserAPI() {
  const userIds = ['123', '456', '赵敏'];
  const userId = userIds[Math.floor(Math.random() * userIds.length)];
  
  const response = http.get(`${BASE_URL}/api/users/${userId}`, {
    headers: {
      'Connection': 'keep-alive',
      'Accept': 'application/json',
    }
  });
  
  const success = check(response, {
    '用户API状态码为200或404': (r) => r.status === 200 || r.status === 404,
    '用户API响应时间<1500ms': (r) => r.timings.duration < 1500,
  });
  
  updateMetrics(success, response);
}

// 测试Hello API
function testHelloAPI() {
  const name = testUsers[Math.floor(Math.random() * testUsers.length)];
  const response = http.get(`${BASE_URL}/hello/${name}`, {
    headers: {
      'Connection': 'keep-alive',
      'Accept': 'application/json',
    }
  });
  
  const success = check(response, {
    'Hello API状态码为200': (r) => r.status === 200,
    'Hello API响应时间<1000ms': (r) => r.timings.duration < 1000,
    'Hello响应包含名字': (r) => r.body && r.body.includes(name),
  });
  
  updateMetrics(success, response);
}

// 测试搜索API
function testSearchAPI() {
  const query = searchQueries[Math.floor(Math.random() * searchQueries.length)];
  const type = Math.random() > 0.5 ? 'framework' : 'library';
  
  const response = http.get(`${BASE_URL}/search?q=${query}&type=${type}`, {
    headers: {
      'Connection': 'keep-alive',
      'Accept': 'application/json',
    }
  });
  
  const success = check(response, {
    '搜索API状态码为200': (r) => r.status === 200,
    '搜索API响应时间<2000ms': (r) => r.timings.duration < 2000,
  });
  
  updateMetrics(success, response);
}

// 统一的指标更新函数
function updateMetrics(success, response) {
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
  
  // 记录连接信息
  if (response.status >= 200 && response.status < 300) {
    connectionsActive.add(1);
  }
}

// 设置阶段
export function setup() {
  console.log('🚀 开始Gecko Web Framework持续连接压力测试');
  console.log(`📊 目标服务器: ${BASE_URL}`);
  console.log('🔗 测试模式: 最大20000个持续连接，每个连接发送多个请求');
  console.log('⏱️  预计测试时间: 约28分钟');
  
  // 预热服务器
  console.log('🔥 预热服务器...');
  const warmupResponse = http.get(`${BASE_URL}/ping`);
  if (warmupResponse.status !== 200) {
    throw new Error(`服务器预热失败，状态码: ${warmupResponse.status}`);
  }
  
  console.log('✅ 服务器预热完成，开始正式测试...');
  return { 
    startTime: new Date(),
    serverVersion: warmupResponse.headers['Server'] || 'Unknown'
  };
}

// 拆卸阶段
export function teardown(data) {
  const endTime = new Date();
  const duration = (endTime - data.startTime) / 1000;
  const minutes = Math.floor(duration / 60);
  const seconds = Math.floor(duration % 60);
  
  console.log('🏁 Gecko Web Framework持续连接测试完成');
  console.log(`⏰ 总测试时间: ${minutes}分${seconds}秒`);
  console.log(`🖥️  服务器版本: ${data.serverVersion}`);
  console.log('�� 详细统计数据请查看K6报告');
} 
