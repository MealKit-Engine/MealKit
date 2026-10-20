// Penpot 도형의 스냅샷 (JSON). snapshot.ts가 플러그인 안에서 Penpot API로 만들고, convert.ts가 .ui로 바꾼다.
// 변환기는 Penpot API를 모르므로 Node에서 고정 데이터(test/fixtures)로 시험한다.

export type Sizing = 'fix' | 'fill' | 'auto';
export type AlignValue = 'start' | 'end' | 'center' | 'stretch';
export type ContentValue = 'start' | 'center' | 'end' | 'space-between' | 'space-around' | 'space-evenly' | 'stretch';

export interface SColor {
  hex: string; // #rrggbb
  opacity: number; // 0..1
  token?: string; // 디자인 토큰 이름 (테마 토큰 $이름으로)
}

export interface SGradient {
  type: 'linear' | 'radial';
  startX: number; // 상자 기준 0..1
  startY: number;
  endX: number;
  endY: number;
  width: number; // 원형: 가로 대 세로 비
  stops: { hex: string; opacity: number; offset: number }[];
}

export interface SFill {
  color?: SColor;
  gradient?: SGradient;
  image?: boolean;
}

export interface SStroke {
  color?: SColor;
  gradient?: boolean;
  width: number;
  alignment: 'center' | 'inner' | 'outer';
  style: string; // solid, dashed, dotted, ...
}

export interface SShadow {
  style: 'drop-shadow' | 'inner-shadow';
  offsetX: number;
  offsetY: number;
  blur: number;
  spread: number;
  color: SColor;
}

export interface STextRun {
  start: number;
  end: number;
  characters: string;
  fontFamily: string;
  fontWeight: string;
  fontStyle: string; // normal | italic
  fontSize: number;
  lineHeight: number; // 글자 크기의 배수
  letterSpacing: number;
  textTransform: string; // none | uppercase | lowercase | capitalize
  textDecoration: string; // none | underline | line-through
  fill?: SColor;
  gradientFill?: boolean;
}

export interface SText {
  characters: string;
  growType: 'fixed' | 'auto-width' | 'auto-height';
  align: string; // left | center | right | justify
  verticalAlign: string; // top | center | bottom
  runs: STextRun[];
}

export interface STrack {
  type: 'flex' | 'fixed' | 'percent' | 'auto';
  value: number;
}

export interface SLayout {
  kind: 'flex' | 'grid';
  dir: string; // flex: row | row-reverse | column | column-reverse, grid: row | column
  wrap: boolean;
  rowGap: number;
  columnGap: number;
  padding: [number, number, number, number]; // 위 오른쪽 아래 왼쪽
  justifyContent: ContentValue;
  alignItems: AlignValue;
  alignContent: ContentValue;
  justifyItems: AlignValue;
  rows: STrack[];
  columns: STrack[];
}

export interface SChild {
  absolute: boolean;
  horizontalSizing: Sizing;
  verticalSizing: Sizing;
  alignSelf: string; // auto | start | center | end | stretch
  justifySelf: string;
  margin: [number, number, number, number];
  minWidth: number | null;
  maxWidth: number | null;
  minHeight: number | null;
  maxHeight: number | null;
}

export interface SCell {
  row: number; // 1부터
  column: number;
  rowSpan: number;
  columnSpan: number;
}

// 플러그인 패널에서 정하는 엔진 값 (도형의 플러그인 데이터 "mealkit", JSON)
export interface MealkitData {
  visible?: string;
  disabled?: string;
  widthRatio?: string;
  colorField?: string;
  widget?: 'button' | 'toggle' | 'slider' | 'select' | 'textInput' | 'scroll' | 'list';
  action?: string;
  field?: string;
  min?: number;
  max?: number;
  step?: number;
  options?: string; // 문자열 키, 쉼표로
  placeholder?: string;
  maxLength?: number;
  scrollHorizontal?: boolean;
  focusUp?: string;
  focusDown?: string;
  focusLeft?: string;
  focusRight?: string;
  slice?: [number, number, number, number]; // 9-slice 여백 (왼쪽 위 오른쪽 아래, 구운 PNG의 논리 픽셀)
  overflow?: 'ellipsis' | 'clip';
  maxLines?: number;
  wrap?: 'word' | 'syllable' | 'none';
  transition?: number;
  bake?: boolean; // 언제나 PNG로 굽는다
  key?: string; // 글자: 문자열 키 (비면 레이어 이름이 키 모양일 때 그것)
  // 보드 (화면) 전용
  navigation?: boolean;
  firstFocus?: string;
  back?: string;
}

export interface SNode {
  id: string;
  name: string;
  type: string; // board | rectangle | ellipse | text | group | path | boolean | image | svg-raw
  x: number; // 부모 기준
  y: number;
  absX: number; // 내보내는 보드 기준
  absY: number;
  width: number;
  height: number;
  rotation: number;
  opacity: number;
  blendMode: string;
  hidden: boolean;
  clip: boolean;
  mask: boolean; // 마스크 그룹
  constraintsH: string; // left | right | leftright | center | scale
  constraintsV: string; // top | bottom | topbottom | center | scale
  boardSizingH?: 'auto' | 'fix'; // 레이아웃이 있는 보드: 내용에 맞춤
  boardSizingV?: 'auto' | 'fix';
  fills: SFill[];
  strokes: SStroke[];
  shadows: SShadow[];
  blur: boolean;
  backgroundBlur: boolean;
  radius: [number, number, number, number]; // 왼쪽 위, 오른쪽 위, 오른쪽 아래, 왼쪽 아래
  radiusToken?: string;
  layout?: SLayout;
  child?: SChild; // 부모에 레이아웃이 있을 때
  cell?: SCell; // 부모가 Grid일 때
  text?: SText;
  data?: MealkitData;
  states?: { state: string; node: SNode }[]; // 컴포넌트 variants의 상태 (hover, pressed ...)
  children: SNode[];
}

// 내보내기 결과
export interface Bake {
  id: string; // 굽는 도형 id
  path: string; // assets 기준 PNG 경로
  scale: number;
  skipChildren: boolean; // 배경만 (자식은 따로 변환)
}

export interface StringEntry {
  key: string;
  text: string;
}

export interface Box {
  id: string;
  name: string;
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface Result {
  ui: string;
  bakes: Bake[];
  strings: StringEntry[];
  boxes: Box[];
  warnings: string[];
  errors: string[];
  nodeCount: number;
}

export interface Options {
  file: string; // assets 기준 .ui 경로 (ui/pause.ui)
  fileName: string; // Penpot 파일 이름
  fileId: string;
  fonts: string[]; // assets/fonts의 파일 이름 (확장자 없이)
  nodeBudget: number; // 화면당 노드 수 예산
}
