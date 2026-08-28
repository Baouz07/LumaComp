// tags.js — list release tags for a repo (paginated).
// Usage: node tags.js <owner/repo> [maxPages]
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
  const [, , repo, maxPages] = process.argv;
  const pages = Math.min(parseInt(maxPages || '3', 10) || 3, 10);
  for (let p = 1; p <= pages; p++) {
    const list = await getJson(`https://api.github.com/repos/${repo}/releases?per_page=100&page=${p}`);
    for (const r of list) console.log(r.tag_name);
    if (list.length < 100) break;
  }
  process.exit(0);
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
