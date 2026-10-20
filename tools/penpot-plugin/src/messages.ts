// 플러그인 코드(plugin.ts, Penpot 안) ↔ 패널(ui.ts, iframe) 메시지

import type { Box, MealkitData, StringEntry } from './model.ts';

export interface LayerInfo {
  id: string;
  name: string;
  type: string;
  topBoard: boolean; // 페이지 바로 밑 보드 (화면 하나)
  data: MealkitData;
  file: string; // 화면 보드: 내보낼 .ui 경로 (assets 기준)
}

export interface BoardInfo {
  id: string;
  name: string;
  file: string;
}

export interface ExportedScreen {
  board: string;
  file: string;
  ui: string;
  images: { path: string; data: Uint8Array; logicalW: number; logicalH: number }[];
  strings: StringEntry[];
  boxes: Box[];
  preview?: Uint8Array; // 보드 전체 PNG (배율 kPreviewScale, 픽셀 비교 art/ui/preview)
  warnings: string[];
  errors: string[];
  nodeCount: number;
}

export type ToUi =
  | { type: 'init'; fileName: string; fileId: string; theme: string }
  | { type: 'selection'; layers: LayerInfo[] }
  | { type: 'boards'; boards: BoardInfo[] }
  | { type: 'progress'; text: string }
  | {
      type: 'exported';
      screens: ExportedScreen[];
      theme?: { ui: string; warnings: string[] };
      source?: { name: string; data: Uint8Array };
      fileName: string;
    }
  | { type: 'error'; message: string };

export type ToPlugin =
  | { type: 'ready' }
  | { type: 'setData'; id: string; data: MealkitData }
  | { type: 'setFile'; id: string; file: string }
  | { type: 'select'; id: string }
  | { type: 'example' }
  | { type: 'export'; boardIds: string[]; fonts: string[]; nodeBudget: number; theme: boolean; source: boolean };
