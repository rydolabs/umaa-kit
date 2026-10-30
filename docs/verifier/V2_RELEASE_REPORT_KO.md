# UMAA Verifier V2 — 로컬 구현 및 검증 결과

2026-10-28 / 범위: 기존 UMAA Kit 소스 스냅샷 + 독립 `verifier/`.

## 구현 범위

1. 기존 V1 독립 시뮬레이터 실행·판정·로그·해시 검증 구조 유지.
2. DDS 프로파일 실행 시 `--dds-network-approved`를 지정하지 않으면 실제
   SUT/Oracle 프로세스를 실행하기 전에 `BLOCKED` 결과 및 증거 기록.
3. `export-results`로 검증 완료된 증거를 Studio 연계용 JSON으로 변환.
   이 형식은 기계 판정이 아닌 결과·출처 교환 형식이다.
4. `scope-report`로 원본 카탈로그의 미승인 요구사항 현황 조회.
   `scope-report --require-ready`는 현재 exit code 2로 배포 게이트 차단.
5. 외부 경로의 stdio 시나리오 사용 차단.
6. 신규 부정·긍정 시험, CI 정의 및 문서 갱신.

## 검증 근거

| 시험 | 실행 결과 | 증거 |
|---|---|---|
| verifier Python 단위·회귀 시험 | 38/38 PASS | `verifier-v2-tests.log` |
| 기존 Simulator Oracle 시험 | 6/6 PASS | 독립 unittest 실행 출력 |
| 기존 Simulator DDS 설정 시험 | 5/5 PASS | 독립 unittest 실행 출력 |
| 기존 SDK 전체 CI | PASS (Python 38/38, CTest 14/14, stdio 4/4) | `sdk-full-ci.log` 및 `summary.json` |
| 기존 C++ Mock Host 기반 Verifier suite | 4/4 PASS | `stdio-suite/suite-verdict.json` |
| Verifier 증거 무결성 | PASS | `verify-evidence` 명령 |
| Studio JSON export 및 오프라인 HTML | PASS | `studio-result.json`, `report.html` |
| 승인되지 않은 DDS 테스트망에 대한 실행 차단 | BLOCKED (exit 2, 의도된 정상 안전동작) | `dds-unapproved/verdict.json` |
| 공식 UMAA 적합성 배포 게이트 | BLOCKED (exit 2, 의도된 차단) | `scope-gate-blocked.json` |

이번 검증은 **실제 Cyclone DDS 통신 실행 검증이 아니다**. 위 stdio 4건은
개발용 mock/독립 Oracle 경로다. 기존 공식 DDS CI 기록은 이번 로컬
V2 실행의 증거로 재사용하거나 재실행했다고 주장하지 않는다.

## 제한 및 잔여 업무

- 공식 Compliance·Component Definitions·ICD의 검토 및 승인된 요구사항
  시험 매핑이 없고 목표 Component도 확정되지 않아 적합성 게이트 차단.
- 이 환경에는 실제 CycloneDDS 및 공식 TypeSupport 실행 바이너리 미확보.
- 라이선스가 필요한 RTI Connext 벤더 교차시험 미실행.
- 다중 사용자 서버/보안 격리/PostgreSQL/Web UI/운영망 연결 미구현.
- SHA-256은 변조 탐지·일관성 확인 수단이며, 서명·불변 증거 저장소가 아님.
- `--dds-network-approved`는 운영자의 선언일 뿐, 네트워크 격리·도메인
  강제·권한 부여를 구현한 기능이 아니므로 운영망 실행에 사용하지 말 것.

**판정:** V2 로컬 오프라인 확장 기능 충족. 전체 UMAA 검증 도구 완성:
아직 미달성. 원격 저장소 반영 없음.
