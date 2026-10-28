#pragma once

#include <cstdio>
#include <string>
#include <vector>

// 파일·폴더 (UTF-8 경로). 구현은 std::filesystem으로 되는 부분(platform/Paths.cpp)과
// OS 호출이 필요한 부분(platform/win32/Paths.cpp, platform/posix/Paths.cpp)으로 나뉜다.
namespace mealkit {

// 실행 파일이 있는 폴더 (UTF-8, 끝에 구분자 없음)
std::string executableDirectory();

// 실행 파일 폴더와 그 상위 폴더들, 현재 폴더에서 relative를 찾는다.
// 개발 중에는 build\Release\bin 에서 실행해도 저장소의 assets 폴더를 찾을 수 있다. 없으면 빈 문자열.
std::string findAsset(const std::string& relative);

// 에셋 경로(assets 기준, "ui/hud.ui") → 프로젝트 기준 경로("assets/ui/hud.ui"). 에셋 경로를 파일로 바꾸는 곳은 모두 이것을 쓴다.
// 소스 모듈의 에셋 "modules/<이름>/ui/x.ui"는 그 모듈의 assets/ 아래 "modules/<이름>/assets/ui/x.ui" (docs/MODULES.md).
// assets/ 바로 밑에는 종류 폴더만 오므로 modules/로 시작하는 경로가 프로젝트 에셋과 겹치지 않는다.
std::string assetFilePath(const std::string& assetPath);
// findAsset(assetFilePath(assetPath))
std::string findAssetFile(const std::string& assetPath);

// 프로젝트 폴더를 정한다 (범용 에디터가 프로젝트를 열 때). 정하면 findAsset은 이 폴더에서만 찾는다
// (에디터 실행 파일 위쪽의 엔진 저장소 assets를 잘못 집지 않도록).
void setProjectRoot(const std::string& directory);
const std::string& projectRoot();

bool fileExists(const std::string& path);
bool directoryExists(const std::string& path);
std::string fileName(const std::string& path);        // 경로에서 파일 이름만
std::string parentDirectory(const std::string& path); // 경로에서 폴더 부분만

// 파일 전체를 읽고 쓴다 (경로는 UTF-8). 쓰기는 같은 폴더의 임시 파일에 쓴 뒤 바꿔 넣어, 도중에 꺼져도 원본이 깨지지 않는다.
bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);
bool createDirectories(const std::string& path);
bool removeFile(const std::string& path);
// 파일 이름 바꾸기·옮기기 (to의 폴더는 있어야 한다, to가 있으면 실패)
bool moveFile(const std::string& from, const std::string& to);
// 폴더 안 파일 이름들 (하위 폴더 제외, 이름 순). extension(".txt")을 주면 그 확장자만
std::vector<std::string> listFiles(const std::string& directory, const char* extension = nullptr);
// 폴더 안 하위 폴더 이름들 (이름 순)
std::vector<std::string> listDirectories(const std::string& directory);
// UTF-8 경로로 fopen (mode는 "rb", "wb" 등). 실패하면 nullptr.
FILE* openFile(const std::string& path, const char* mode);
// 마지막 수정·만든 시각 (OS 단위, 비교와 formatFileTime용, 없으면 0)
unsigned long long fileWriteTime(const std::string& path);
unsigned long long fileCreationTime(const std::string& path);
// 위 시각을 지역 시각 "2026-10-18" 또는 "2026-10-18 14:05"로. 0이면 "-".
std::string formatFileTime(unsigned long long time, bool withTime);

// 사용자별 앱 데이터 폴더의 바탕 (Windows %LOCALAPPDATA%, Linux $XDG_DATA_HOME 또는 ~/.local/share,
// macOS ~/Library/Application Support). 엔진·게임은 이 아래에 자기 폴더를 만든다. 모르면 실행 파일 폴더.
std::string userDataDirectory();
// 사용자 문서 폴더 (새 프로젝트 기본 위치). 모르면 빈 문자열.
std::string documentsDirectory();

} // namespace mealkit
