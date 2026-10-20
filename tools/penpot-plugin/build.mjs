// 플러그인 빌드: src → dist (dist는 저장소에 올린다. 에디터가 Node.js 없이 그대로 준다).
//   npm install   (처음 한 번, tools/penpot-plugin에서)
//   npm run build
// dist/plugin.js의 __MEALKIT_BASE__는 에디터 가져오기 서버가 줄 때 자기 주소로 바꾼다.

import { build } from 'esbuild';
import { copyFileSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';

const common = { bundle: true, format: 'iife', target: 'es2020', charset: 'utf8', legalComments: 'none', logLevel: 'warning' };
mkdirSync('dist', { recursive: true });
await build({ ...common, entryPoints: ['src/plugin.ts'], outfile: 'dist/plugin.js' });
const ui = await build({ ...common, entryPoints: ['src/ui.ts'], write: false });
const script = ui.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const html = readFileSync('src/ui.html', 'utf8').replace('<script>/*UI_SCRIPT*/</script>', () => `<script>\n${script}</script>`);
writeFileSync('dist/ui.html', html);
copyFileSync('src/manifest.json', 'dist/manifest.json');
copyFileSync('src/icon.png', 'dist/icon.png');
console.log('dist: manifest.json, plugin.js, ui.html, icon.png');
