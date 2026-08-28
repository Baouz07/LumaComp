// dl.js — download a URL to a file through the local HTTP proxy using https-proxy-agent
// (Node/BoringSSL TLS, which bypasses the broken system schannel).
// Usage: node dl.js <url> <outputPath>   ("-" = stdout)
'use strict';
const https = require('https');
const fs = require('fs');
const NM = 'C:/Users/Shiyu/AppData/Local/Programs/DSH Desktop/resources/app.asar.unpacked/node_modules';
const { HttpsProxyAgent } = require(NM + '/https-proxy-agent');
const agent = new HttpsProxyAgent('http://127.0.0.1:7897');

function get(url, outPath, depth) {
  return new Promise((resolve, reject) => {
    if (depth > 12) return reject(new Error('too many redirects'));
    let u;
    try { u = new URL(url); } catch (e) { return reject(e); }
    const req = https.request({
      host: u.hostname,
      path: u.pathname + u.search,
      method: 'GET',
      agent,
      headers: { 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) dsh-dl/1.0' }
    }, res => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        res.resume();
        const next = new URL(res.headers.location, url).href;
        process.stderr.write(`  -> ${res.statusCode} ${next}\n`);
        return resolve(get(next, outPath, depth + 1));
      }
      if (res.statusCode !== 200) {
        let body = '';
        res.on('data', c => { body = (body + c.toString('utf8')).slice(0, 1500); });
        res.on('end', () => reject(new Error(`HTTP ${res.statusCode} for ${url} :: ${body}`)));
        return;
      }
      if (outPath === '-') {
        res.pipe(process.stdout);
        res.on('end', () => resolve());
        return;
      }
      const total = parseInt(res.headers['content-length'] || '0', 10);
      const ws = fs.createWriteStream(outPath);
      let got = 0;
      const mb = () => (got / 1048576).toFixed(1);
      res.on('data', c => {
        got += c.length;
        if (Math.floor(got / (16 * 1048576)) !== Math.floor((got - c.length) / (16 * 1048576)))
          process.stderr.write(`  ${mb()} MB${total ? ' / ' + (total / 1048576).toFixed(1) + ' MB' : ''}\n`);
      });
      res.pipe(ws);
      ws.on('finish', () => { process.stderr.write(`saved ${outPath} (${got} bytes)\n`); resolve(); });
      ws.on('error', reject);
      res.on('error', reject);
    });
    req.on('error', reject);
    req.end();
  });
}

const [, , url, out] = process.argv;
if (!url || !out) { console.error('usage: node dl.js <url> <out>'); process.exit(3); }
process.stderr.write(`GET ${url}\n`);
get(url, out).then(() => process.exit(0)).catch(e => { console.error('FATAL', e.message); process.exit(1); });
