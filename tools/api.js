// api.js — GET a URL (via proxy agent) and print status + body (truncated).
// Usage: node api.js <url> [maxChars]
'use strict';
const https = require('https');
const NM = 'C:/Users/Shiyu/AppData/Local/Programs/DSH Desktop/resources/app.asar.unpacked/node_modules';
const { HttpsProxyAgent } = require(NM + '/https-proxy-agent');
const agent = new HttpsProxyAgent('http://127.0.0.1:7897');
const u = new URL(process.argv[2]);
const max = Number(process.argv[3] || 20000);
https.request({
  host: u.hostname, path: u.pathname + u.search, method: 'GET', agent,
  headers: { 'User-Agent': 'Mozilla/5.0', 'Accept': 'application/vnd.github+json' }
}, res => {
  let d = '';
  res.on('data', c => { if (d.length < max + 2000) d += c; });
  res.on('end', () => { console.error('STATUS', res.statusCode); console.log(d.slice(0, max)); process.exit(res.statusCode === 200 ? 0 : 1); });
}).on('error', e => { console.error('ERR', e.message); process.exit(1); }).end();
