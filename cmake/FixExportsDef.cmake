# mealkit.dll의 WINDOWS_EXPORT_ALL_SYMBOLS가 만든 exports.def에서 잘못된 줄을 지운다 (PRE_LINK, cmake -P).
# MSVC CRT의 약한 별칭 기호(_Avx2WmemEnabledWeakValue 등)가 "==" 같은 빈 별칭 줄로 나와 링크가 실패한다.
# 입력: DEF_FILE
if(NOT EXISTS "${DEF_FILE}")
    return()
endif()
file(STRINGS "${DEF_FILE}" lines)
set(out "")
foreach(line IN LISTS lines)
    string(STRIP "${line}" stripped)
    # 이름 없이 = 만 있는 줄, 그리고 CRT 내부 기호(내보낼 이유가 없다)는 뺀다
    if(stripped MATCHES "^=+$" OR stripped MATCHES "^(_Avx2WmemEnabledWeakValue|__isa_available_default)")
        continue()
    endif()
    string(APPEND out "${line}\n")
endforeach()
file(WRITE "${DEF_FILE}" "${out}")
