// "예제 화면 만들기": 지금 페이지에 일시정지 메뉴 모양의 보드를 Penpot API로 그린다.
// 디자이너가 형식(Flex·Grid·글자 키·버튼 바인딩·구울 아이콘)을 보는 예제이자, 플러그인 시험용 화면이다.

import type { Board, Shape, Text } from '@penpot/plugin-types';
import { kDataKey } from './snapshot.ts';

const font = 'Pretendard'; // 엔진 예제 프로젝트의 글꼴 (Penpot 팀 글꼴로 올려 두면 쓴다)

function text(characters: string, name: string, size: number, color: string): Text {
  const t = penpot.createText(characters);
  if (!t) throw new Error('createText failed');
  t.name = name;
  const f = penpot.fonts.findByName(font);
  if (f) f.applyToText(t, f.variants.find((v) => v.fontWeight === '500') ?? undefined);
  t.fontSize = String(size);
  t.fills = [{ fillColor: color, fillOpacity: 1 }];
  t.growType = 'auto-width';
  return t;
}

function board(name: string, w: number, h: number, color: string | null, opacity = 1): Board {
  const b = penpot.createBoard();
  b.name = name;
  b.resize(w, h);
  b.fills = color ? [{ fillColor: color, fillOpacity: opacity }] : [];
  return b;
}

function fillWidth(s: Shape) {
  if (s.layoutChild) s.layoutChild.horizontalSizing = 'fill';
}

export function createExample(): Board {
  const root = board('pause', 1920, 1080, '#202634');
  root.x = 0;
  root.y = 0;
  root.setPluginData(kDataKey, JSON.stringify({ navigation: true, firstFocus: 'resume', back: 'pause.resume' }));

  const dim = penpot.createRectangle();
  dim.name = 'dim';
  dim.resize(1920, 1080);
  dim.fills = [{ fillColor: '#05070c', fillOpacity: 0.6 }];
  root.appendChild(dim);
  dim.x = root.x;
  dim.y = root.y;
  dim.constraintsHorizontal = 'leftright';
  dim.constraintsVertical = 'topbottom';

  const panel = board('panel', 500, 400, '#10141c', 0.8);
  root.appendChild(panel);
  panel.x = root.x + 710;
  panel.y = root.y + 300;
  panel.borderRadius = 16;
  panel.strokes = [{ strokeColor: '#ffffff', strokeOpacity: 0.15, strokeWidth: 1, strokeAlignment: 'inner', strokeStyle: 'solid' }];
  panel.shadows = [{ style: 'drop-shadow', offsetX: 0, offsetY: 12, blur: 40, spread: 0, color: { color: '#000000', opacity: 0.67 } }];
  panel.constraintsHorizontal = 'center';
  panel.constraintsVertical = 'center';
  const flex = panel.addFlexLayout();
  flex.dir = 'column';
  flex.rowGap = 14;
  flex.topPadding = flex.bottomPadding = 32;
  flex.leftPadding = flex.rightPadding = 36;
  panel.verticalSizing = 'auto';

  const title = text('Paused', 'pause.title', 40, '#f2f4f8');
  panel.appendChild(title);
  fillWidth(title);
  title.getRange(0, title.characters.length).align = 'center'; // 정렬은 문단에 (도형의 align만 바꾸면 그리지 않는다)

  for (const [name, label, action, color] of [
    ['resume', 'Resume', 'pause.resume', '#ffc83d'],
    ['leave', 'Leave', 'pause.leave', '#2a3242'],
  ] as const) {
    const button = board(name, 428, 60, color);
    button.borderRadius = 12;
    const row = button.addFlexLayout();
    row.dir = 'row';
    row.justifyContent = 'center';
    row.alignItems = 'center';
    button.setPluginData(kDataKey, JSON.stringify({ widget: 'button', action }));
    panel.appendChild(button);
    fillWidth(button);
    const t = text(label, 'pause.' + name, 28, name === 'resume' ? '#10141c' : '#f2f4f8');
    button.appendChild(t);
  }

  // Grid: 왼쪽 열은 남는 자리, 오른쪽은 120 고정. 아래 칸은 두 열을 합친다
  const stats = board('stats', 428, 100, null);
  panel.appendChild(stats);
  fillWidth(stats);
  const grid = stats.addGridLayout();
  grid.addColumn('flex', 1);
  grid.addColumn('fixed', 120);
  grid.addRow('auto');
  grid.addRow('auto');
  grid.rowGap = 8;
  grid.columnGap = 16;
  stats.verticalSizing = 'auto';
  const cells: [string, number, number][] = [
    ['a', 1, 1],
    ['b', 1, 2],
    ['c', 2, 1],
  ];
  for (const [name, row, col] of cells) {
    const r = penpot.createRectangle();
    r.name = name;
    r.resize(60, 24);
    r.fills = [{ fillColor: '#ffffff', fillOpacity: 0.15 }];
    r.borderRadius = 4;
    grid.appendChild(r, row, col);
    if (name !== 'b') fillWidth(r);
  }
  const wide = penpot.createRectangle();
  wide.name = 'wide';
  wide.resize(60, 16);
  wide.fills = [{ fillColor: '#ffc83d', fillOpacity: 1 }];
  grid.appendChild(wide, 2, 2);

  // 아이콘 (패스 → PNG로 굽는다)
  const icon = penpot.createShapeFromSvg(
    '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"><path d="M6 4h4v16H6zM14 4h4v16h-4z" fill="#ffffff"/></svg>',
  );
  if (icon) {
    icon.name = 'pause_icon';
    root.appendChild(icon);
    icon.x = root.x + 940;
    icon.y = root.y + 240;
  }
  panel.bringToFront(); // dim 위에

  // 디자인 토큰: 색 accent를 [계속] 버튼 채우기에 (내보내면 theme.ui의 color accent, .ui에는 $accent)
  try {
    const catalog = penpot.library.local.tokens;
    const set = catalog.sets.find((s) => s.name === 'mealkit') ?? catalog.addSet({ name: 'mealkit', active: true });
    if (!set.active) set.toggleActive();
    const accent = set.tokens.find((t) => t.name === 'accent') ?? set.addToken({ type: 'color', name: 'accent', value: '#ffc83d' });
    const resume = panel.children.find((c) => c.name === 'resume');
    if (resume) resume.applyToken(accent, ['fill']);
  } catch {
    // 토큰이 없는 Penpot 버전: 예제는 색 값만 쓴다
  }
  return root;
}
