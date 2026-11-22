// docs/02_design/art-pipeline.md A01: SVG → PNG 변환(build)과 검수용 모아 보기(sheet)
//   npm run build                  Art/Source/**/*.svg → Art/Export/**/*.png
//   npm run sheet -- <PNG 폴더>     레포 루트 기준 경로. 결과는 Art/Export/_sheets/<폴더명>.png
import fs from 'node:fs';
import path from 'node:path';
import sharp from 'sharp';

const ROOT = path.resolve(import.meta.dirname, '../..');
const SOURCE = path.join(ROOT, 'Art/Source');
const EXPORT = path.join(ROOT, 'Art/Export');
const SHEETS = path.join(EXPORT, '_sheets');

// 작은 사본 목록: Art/Source 기준 경로(/ 구분) → [[가로, 세로, 이름 뒤에 붙일 말], ...]
// 진열 제품 겹침 · 말풍선용 재료 80×80 (_S), 앱 아이콘 UE Android 칸 크기 36 · 48 · 72 · 96 (art-style.md 2장)
const SMALL = {
  'Ingredient/T_Ing_RedBean.svg': [[80, 80, '_S']],
  'Ingredient/T_Ing_Yogurt.svg': [[80, 80, '_S']],
  'Ingredient/T_Ing_DubaiChoco.svg': [[80, 80, '_S']],
  'Ingredient/T_Ing_Strawberry.svg': [[80, 80, '_S']],
  'Ingredient/T_Ing_Blueberry.svg': [[80, 80, '_S']],
  'Ingredient/T_Ing_Pineapple.svg': [[80, 80, '_S']],
  'Ingredient/T_Ing_Mango.svg': [[80, 80, '_S']],
  'App/T_AppIcon.svg': [[36, 36, '_36'], [48, 48, '_48'], [72, 72, '_72'], [96, 96, '_96']],
};

// sharp는 SVG를 density 72에서 SVG 단위 1 = 1px로 그린다 (sharp/lib/index.d.ts SharpOptions.density, 기본 72)
const BASE_DENSITY = 72;
const SIZE_STEP = 4;

const SHEET_WIDTH = 1092;
const SHEET_MAX_COLS = 4;
const SHEET_PAD = 12;
const SHEET_FONT = 14;
const SHEET_LABEL_HEIGHT = 40;
const SHEET_BACKGROUND = '#D8D8D8';

async function build() {
  const css = fs.readFileSync(path.join(SOURCE, '_style.css'), 'utf8');
  const files = fs.readdirSync(SOURCE, { recursive: true }).filter((f) => f.endsWith('.svg'));
  for (const file of files) {
    // readdirSync는 Windows에서 \ 구분자를 돌려주므로 SMALL 키와 맞추려고 / 로 바꾼다
    const rel = file.replaceAll('\\', '/');
    const text = fs.readFileSync(path.join(SOURCE, rel), 'utf8');
    const openTag = text.match(/<svg\b[^>]*>/)?.[0];
    if (!openTag) {
      throw new Error(`${rel}: <svg> 태그가 없다`);
    }
    const width = Number(openTag.match(/\swidth="(\d+)(?:px)?"/)?.[1]);
    const height = Number(openTag.match(/\sheight="(\d+)(?:px)?"/)?.[1]);
    if (!width || !height) {
      throw new Error(`${rel}: <svg>에 정수 px width · height가 없다`);
    }
    if (width % SIZE_STEP !== 0 || height % SIZE_STEP !== 0) {
      console.warn(`경고 ${rel}: 크기가 ${SIZE_STEP}의 배수가 아니다 — ${width}×${height} (art-pipeline.md 3.1)`);
    }

    // CSS 안의 > · & 가 XML로 해석되지 않게 CDATA로 감싼다. 치환 문자열의 $ 패턴을 피하려고 함수로 넘긴다
    const styled = text.replace(openTag, () => `${openTag}<style><![CDATA[${css}]]></style>`);
    const svg = Buffer.from(styled);
    const out = path.join(EXPORT, rel.replace(/\.svg$/, '.png'));
    fs.mkdirSync(path.dirname(out), { recursive: true });
    await sharp(svg).png().toFile(out);
    console.log(`${rel} → ${width}×${height}`);

    for (const [smallWidth, smallHeight, suffix] of SMALL[rel] ?? []) {
      // 큰 PNG를 줄이지 않고 SVG를 작은 크기로 다시 그린다 (art-pipeline.md 3.1 축소 2배 이내)
      const density = BASE_DENSITY * smallWidth / width;
      await sharp(svg, { density }).resize(smallWidth, smallHeight).png().toFile(out.replace(/\.png$/, `${suffix}.png`));
      console.log(`${rel} → ${smallWidth}×${smallHeight} (${suffix})`);
    }
  }
}

async function sheet(dirArg) {
  if (!dirArg) {
    throw new Error('사용법: npm run sheet -- <PNG 폴더 (레포 루트 기준)>');
  }
  const dir = path.resolve(ROOT, dirArg);
  const files = fs.readdirSync(dir).filter((f) => f.endsWith('.png')).sort();
  if (files.length === 0) {
    throw new Error(`${dirArg}: PNG가 없다`);
  }

  const cols = Math.min(files.length, SHEET_MAX_COLS);
  const cell = Math.floor((SHEET_WIDTH - SHEET_PAD * (cols + 1)) / cols);
  const overlays = [];
  const labels = [];
  let top = SHEET_PAD;
  for (let i = 0; i < files.length; i += cols) {
    const row = await Promise.all(files.slice(i, i + cols).map(async (name) => {
      const src = path.join(dir, name);
      const meta = await sharp(src).metadata();
      // 칸보다 작은 이미지는 늘리지 않는다 — 원본 픽셀 그대로 검수
      const { data, info } = await sharp(src)
        .resize({ width: cell, withoutEnlargement: true })
        .png()
        .toBuffer({ resolveWithObject: true });
      return { name, meta, data, info };
    }));
    const rowHeight = Math.max(...row.map((tile) => tile.info.height));
    row.forEach((tile, col) => {
      const left = SHEET_PAD + col * (cell + SHEET_PAD);
      const center = left + cell / 2;
      const nameY = top + rowHeight + SHEET_FONT + 4;
      overlays.push({ input: tile.data, left: left + Math.floor((cell - tile.info.width) / 2), top });
      labels.push(`<text x="${center}" y="${nameY}">${tile.name.replace(/\.png$/, '')}</text>`);
      labels.push(`<text x="${center}" y="${nameY + SHEET_FONT + 4}">${tile.meta.width}×${tile.meta.height}</text>`);
    });
    top += rowHeight + SHEET_LABEL_HEIGHT + SHEET_PAD;
  }

  const height = top;
  const labelSvg = `<svg xmlns="http://www.w3.org/2000/svg" width="${SHEET_WIDTH}" height="${height}" `
    + `font-family="sans-serif" font-size="${SHEET_FONT}" fill="#202020" text-anchor="middle">${labels.join('')}</svg>`;
  overlays.push({ input: Buffer.from(labelSvg), left: 0, top: 0 });

  fs.mkdirSync(SHEETS, { recursive: true });
  const out = path.join(SHEETS, `${path.basename(dir)}.png`);
  await sharp({ create: { width: SHEET_WIDTH, height, channels: 4, background: SHEET_BACKGROUND } })
    .composite(overlays)
    .png()
    .toFile(out);
  console.log(`${path.relative(ROOT, out)} (${files.length}장, ${SHEET_WIDTH}×${height})`);
}

const [command, arg] = process.argv.slice(2);
switch (command) {
  case 'build':
    await build();
    break;
  case 'sheet':
    await sheet(arg);
    break;
  default:
    throw new Error('사용법: node art.mjs build | node art.mjs sheet <PNG 폴더 (레포 루트 기준)>');
}
