// tree.js — print file paths of a repo ref matching a pattern.
// Usage: node tree.js <owner/repo> <ref> <pathRegex>
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
  const [, , repo, ref, pat] = process.argv;
  const rx = new RegExp(pat || '.');
  const tree = await getJson(`https://api.github.com/repos/${repo}/git/trees/${encodeURIComponent(ref)}?recursive=1`);
  if (tree.truncated) console.error('(tree truncated)');
  for (const t of tree.tree || []) {
    if (t.type === 'blob' && rx.test(t.path)) console.log(t.path);
  }
  process.exit(0);
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
