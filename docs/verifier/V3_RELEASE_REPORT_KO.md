# UMAA Kit Verifier V3 및 Studio QEA Adapter 초기 증분 — 로컬 검증 보고서

기준: 2026-10-28. 출처: 제공된 V2 통합 소스 ZIP. 원격 GitHub 변경 없음.

## 구현 범위

- Verifier V3 (`verifier/`): 성공 종료 코드만으로 통과 판정을 내리지 않도록
  DDS 프로파일별 독립 Oracle/호스트 성공 표식 재검사. 정상 stdio 시험의
  시나리오를 자체 증거로 복사하여 해시 검증과 의미 일치 검증 수행.
  `verdict.json` schemaVersion `1.1` (기존 1.0 읽기 호환 유지).
- 개발용 실제 DDS 시나리오 계획 5종: EngineControl, AnchorControl,
  AnchorControl Extended, AnchorSpecs, Reviewed Discovery. 실행 바이너리,
  TypeSupport, DDS 설치/시험망은 포함하지 않음.
- Studio QEA Adapter 초기 구현 (`studio/`): EA형 SQLite QEA 파일을
  읽기 전용으로 열고 Package, Element, Connector, Attribute, Diagram
  일부를 추출해 GUID 및 원본 파일 해시와 함께 JSON 스냅샷 저장.
  스냅샷 간 GUID 기반 변경 비교 및 참조 이상 탐지. 실 QEA가 아닌 합성
  EA 유사 SQLite Fixture만 검증됨.
- GitHub Actions의 Verifier CI에 Studio 단위시험 연결.
- 기존 `sdk/`, `dds/`, `simulator/`, `catalog/`, `tools/` 소스 불변.

## 실행한 실제 시험과 판정

| 항목 | 로컬 결과 | 증거 파일 |
|---|---|---|
| Verifier unit/regression | 44/44 PASS | `verifier-tests.log` |
| Studio synthetic QEA fixture | 6/6 PASS | `studio-tests.log` |
| Simulator Oracle | 6/6 PASS | `simulator-tests.log` |
| Simulator DDS config tests (실 DDS 아님) | 5/5 PASS | `simulator-dds-config-tests.log` |
| SDK 전체 CI | PASS, 20단계, Python 38건, CTest 14건, stdio 4건 | `sdk-ci.log`, CI summary |
| C++ Mock Host와 Verifier V3 | 4/4 PASS | `stdio-suite/suite-verdict.json` |
| 증거 재검사 | PASS | `integrity.log` |
| Studio 연계 결과 JSON | 비규범 JSON 생성 | `studio-result.json` |
| DDS 실행 승인 누락 | 의도된 BLOCKED (exit 2) | `dds-blocked/verdict.json`, `blocked-exitcodes.txt` |
| 공식 UMAA 적합성 게이트 | 의도된 BLOCKED (exit 2) | `scope-blocked.json`, `blocked-exitcodes.txt` |

`PASS`는 해당 **개발용 시험 범위**에만 적용됨. 별도 독립 승인 없이
공식 UMAA 적합성 통과라고 주장해서는 안 됨. SHA-256은 데이터
일관성 검증이지 신뢰 가능한 서명이나 위변조 불가능한 저장소가 아님.

## 미달성·외부 제약

- 로컬 환경에 Cyclone DDS/승인된 UMAA 6.0 TypeSupport 및 서비스별 실 DDS
  바이너리가 없어 **실 DDS 송수신은 실행하지 못함**.
- RTI Connext 벤더 간 상호운용성 시험 미실행.
- 승인된 공식 규범 요구사항 0건, 적용 Component 미확정이므로
  `scope-report --require-ready`는 BLOCKED 유지.
- 실제 Sparx EA QEA 예제 미제공으로 QEA Import는 아직 PoC 수준.
- Studio PostgreSQL·React UI·DOCX/IDL Generator와 Verifier의 보안 격리
  Worker·다중 사용자 운영 API/UI는 미구현.

## 다음에 필요한 시험 게이트

1. 사용자에게서 제공받은 실제 EA/QEA와 공식 UML 모델 내보내기를 대조하여
   QEA Adapter 정확도를 확인하고 Tagged Value/다이어그램 지원 범위 확정.
2. 공식 Compliance/Component/ICD 원문을 검토해 대상 컴포넌트와
   규범 시험 계약을 승인하고 요구사항과 독립 시험을 연결.
3. 격리된 승인 시험망에 Cyclone DDS/UMAA TypeSupport를 설치한 후
   각 DDS Suite를 실제 SUT/Oracle로 실행하고 증거 수집.
4. 그 후 PostgreSQL/인증·권한·격리 워커/UI 등을 제품 요구사항에 맞게 구현.

## 판정

**V3 로컬 개발 증분 기능은 시험 통과.** 전체 UMAA 검증/개발
플랫폼의 목표는 외부 입력과 추가 구현이 필요해 **미완료**.
