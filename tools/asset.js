// asset.js — print assets of a specific release tag.
// Usage: node asset.js <repo> <tagOrLatest> [nameRegex]
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
  const [, , repo, tag, pattern] = process.argv;
  const ep = tag === 'latest'
    ? `https://api.github.com/repos/${repo}/releases/latest`
    : `https://api.github.com/repos/${repo}/releases/tags/${encodeURIComponent(tag)}`;
  const rel = await getJson(ep);
  console.log(`TAG ${rel.tag_name}`);
  const rx = pattern ? new RegExp(pattern) : null;
  for (const a of rel.assets || []) {
    if (!rx || rx.test(a.name)) console.log(`${a.name}\t${(a.size / 1048576).toFixed(1)} MB\t${a.browser_download_url}`);
  }
  process.exit(0);
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
