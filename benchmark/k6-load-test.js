import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';

/* Custom metrics */
const errorRate = new Rate('errors');
const apiResponseTimes = new Trend('api_response_times');
const requestCounter = new Counter('requests_total');
const connectionsActive = new Counter('connections_active');

/* Server configuration */
const BASE_URL = 'http://localhost:13514';

/* Load test config focused on connection longevity */
export const options = {
  /* Sustained connection stages (up to 20k) */
  stages: [
    { duration: '2m', target: 10000 },     /* Ramp to 10k connections */
    { duration: '2m', target: 50000 },    /* Peak at 50k virtual users */
     {duration: '2m', target: 30000 },    /* Drop down to 30k */
    { duration: '1m', target: 0 },        /* Drain all connections */
  ],
  
  /* Thresholds for long-lived connections */
  thresholds: {
    http_req_duration: ['p(95)<2000'],    /* 95% under 2s */
    http_req_failed: ['rate<0.02'],       /* Failures under 2% */
    errors: ['rate<0.05'],                /* Custom errors under 5% */
    api_response_times: ['p(99)<5000'],   /* 99% responses under 5s */
    requests_total: ['count>100000'],     /* Exceed 100k requests */
  },
  
  /* Connection reuse to mimic browsers */
  userAgent: 'K6-Gecko-LoadTester/1.0',
  noConnectionReuse: false,              /* Keep-Alive enabled */
  maxRedirects: 4,
  batch: 1,                              /* Single request per batch */
  /* httpDebug: 'full',                  Disable for less noise */
};

/* Scenario definitions tuned for connection persistence */
export const scenarios = {
  /* Default scenario: continuous connections and requests */
  continuous_load: {
    executor: 'ramping-vus',
    startVUs: 0,
    stages: [
      { duration: '2m', target: 5000 },
      { duration: '3m', target: 10000 },
      { duration: '3m', target: 15000 },
      { duration: '2m', target: 20000 },
      { duration: '10m', target: 20000 },  /* Hold 20k connections for 10min */
      { duration: '4m', target: 0 },
    ],
    gracefulRampDown: '1m',               /* Graceful shutdown */
  }
};

/* Test data */
const testUsers = ['alice', 'bob', 'charlie', 'diana', 'eve', 'frank', 'grace', 'henry'];
const searchQueries = ['gecko', 'framework', 'performance', 'benchmark', 'test', 'api', 'web', 'server'];

export default function() {
  /* Each virtual connection issues multiple requests */
  const requestsPerConnection = Math.floor(Math.random() * 5) + 3; /* 3-7 requests */
  
  for (let i = 0; i < requestsPerConnection; i++) {
    /* Randomize scenarios to mimic user behavior */
    const testScenarios = [
      testPingAPI,       /* 40% lightweight */
      testHomePage,      /* 25% home page */
      testUserAPI,       /* 20% API */
      testHelloAPI,      /* 10% parameterized */
      testSearchAPI,     /* 5% complex */
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
    
    /* Think time between requests */
    sleep(Math.random() * 0.3 + 0.1); /* 0.1-0.4s */
  }
  
  /* Pause before disconnect to mimic reading time */
  sleep(Math.random() * 2 + 1); /* 1-3s pause */
}

/* Ping endpoint keeps the connection alive */
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

/* Home page test */
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

/* User API test */
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

/* Hello API test */
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

/* Search API test */
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

/* Shared metrics updater */
function updateMetrics(success, response) {
  errorRate.add(!success);
  apiResponseTimes.add(response.timings.duration);
  requestCounter.add(1);
  
  /* Track active connection info */
  if (response.status >= 200 && response.status < 300) {
    connectionsActive.add(1);
  }
}

/* Setup phase */
export function setup() {
  console.log('[START] 开始Gecko Web Framework持续连接压力测试');
  console.log(`[STATS] 目标服务器: ${BASE_URL}`);
  console.log('[LINK] 测试模式: 最大20000个持续连接，每个连接发送多个请求');
  console.log('⏱️  预计测试时间: 约28分钟');
  
  /* Warm up server */
  console.log('[WARMUP] 预热服务器...');
  const warmupResponse = http.get(`${BASE_URL}/ping`);
  if (warmupResponse.status !== 200) {
    throw new Error(`服务器预热失败，状态码: ${warmupResponse.status}`);
  }
  
  console.log('[OK] 服务器预热完成，开始正式测试...');
  return { 
    startTime: new Date(),
    serverVersion: warmupResponse.headers['Server'] || 'Unknown'
  };
}

/* Teardown phase */
export function teardown(data) {
  const endTime = new Date();
  const duration = (endTime - data.startTime) / 1000;
  const minutes = Math.floor(duration / 60);
  const seconds = Math.floor(duration % 60);
  
  console.log('[DONE] Gecko Web Framework持续连接测试完成');
  console.log(`⏰ 总测试时间: ${minutes}分${seconds}秒`);
  console.log(`[HOST]  服务器版本: ${data.serverVersion}`);
  console.log('�� 详细统计数据请查看K6报告');
} 
