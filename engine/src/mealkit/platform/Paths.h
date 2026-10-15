#pragma once

#include <string>

namespace mealkit {

// 실행 파일이 있는 폴더 (UTF-8, 끝에 구분자 없음)
std::string executableDirectory();

// 실행 파일 폴더와 그 상위 폴더들, 현재 폴더에서 relative를 찾는다.
// 개발 중에는 build\Release\bin 에서 실행해도 저장소의 assets 폴더를 찾을 수 있다. 없으면 빈 문자열.
std::string findAsset(const std::string& relative);

// 프로젝트 폴더를 정한다 (범용 에디터가 프로젝트를 열 때). 정하면 findAsset은 이 폴더에서만 찾는다
// (에디터 실행 파일 위쪽의 엔진 저장소 assets를 잘못 집지 않도록).
void setProjectRoot(const std::string& directory);
const std::string& projectRoot();

bool fileExists(const std::string& path);
bool directoryExists(const std::string& path);
std::string fileName(const std::string& path); // 경로에서 파일 이름만

} // namespace mealkit
