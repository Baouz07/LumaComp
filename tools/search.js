// search.js — GitHub repo search; print full_name + description.
// Usage: node search.js <query> [perPage]
'use strict';
const https = require('https');
const NM = 'C:/Users/Shiyu/AppData/Local/Programs/DSH Desktop/resources/app.asar.unpacked/node_modules';
const { HttpsProxyAgent } = require(NM + '/https-proxy-agent');
const agent = new HttpsProxyAgent('http://127.0.0.1:7897');

function getJson(url) {
  return new Promise((resolve, reject) => {
    const u = new URL(url);
    const req = https.request({
      host: u.hostname, path: u.pathname + u.search, method: 'GET', agent,
      headers: { 'User-Agent': 'Mozilla/5.0 dsh-dl', 'Accept': 'application/vnd.github+json' }
    }, res => {
      let d = '';
      res.on('data', c => d += c);
      res.on('end', () => {
        if (res.statusCode !== 200) return reject(new Error('HTTP ' + res.statusCode + ' ' + url));
        try { resolve(JSON.parse(d)); } catch (e) { reject(e); }
      });
    });
    req.on('error', reject);
    req.end();
  });
}

(async () => {
  const [, , q, per] = process.argv;
  const n = parseInt(per || '20', 10);
  const r = await getJson(`https://api.github.com/search/repositories?q=${encodeURIComponent(q)}&per_page=${n}`);
  console.log(`total_count=${r.total_count}`);
  for (const it of r.items || []) {
    console.log(`${it.full_name}  |  ${(it.description || '').slice(0, 80)}`);
  }
  process.exit(0);
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
