// releases.js — print latest release + asset URLs for GitHub repos.
// Usage: node releases.js owner/repo [assetNamePattern]
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
  const [, , repo, pattern] = process.argv;
  if (!repo) { console.error('usage: node releases.js owner/repo [pattern]'); process.exit(3); }
  const rel = await getJson(`https://api.github.com/repos/${repo}/releases/latest`);
  console.log(`REPO ${repo}  tag=${rel.tag_name}  name=${rel.name}`);
  for (const a of rel.assets || []) {
    const hit = !pattern || a.name.includes(pattern);
    console.log(`${hit ? 'HIT ' : '    '}${a.name}  ${(a.size / 1048576).toFixed(1)} MB  ${a.browser_download_url}`);
  }
  process.exit(0);
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
