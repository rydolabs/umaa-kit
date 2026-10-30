# K-UMAA ICD YAML v1alpha1 — 설계 초안

> **상태:** 독립 설계/정적 검증 초안. 공식 UMAA 6.0 규격의 대체물이 아니며
> `officialIdlTypes`, `keysAndQos`, `nativeDdsWire`가 모두 검증된 실행 계약이 아니다.

## 결론: ICD를 "읽을 수 있는 계약 + 원문 근거 + 코드 생성 IR"로 표현

기존의 53서비스/113 Topic JSON 카탈로그는 ***검토된 리터럴 식별자***
정보만 갖는다. 실제 ICD 본문에는 서비스 목적, Operation 요청/응답 표,
각 operation의 설명과 Namespace/Topic/Data Type, 속성 표의 필수/선택,
기반 타입, Flow Control, Large Collection, Generalization/Specialization,
DDS에서 처리되는 `query...⊕`/`cancel...⊕` 동작이 더 있다.
따라서 카탈로그 JSON을 YAML로 바꾸는 것만으로 원문 보존과 자동 생성이
완성되지 않는다.

**설계의 실제 근거**: 기존 `catalog/umaa6-six-icd-reviewed.json` 및
RTI Community의 6.0 공개 전사 `ecb923646e7f3fe569b773c3946444e793d8a8be`
(EO, MO, SA); *공식 AUVSI 원본 ICD/IDL과 Component Definitions는 최종 검증 입력*이다.
전사본은 중간 검토 자료이며 원본 타입·키·QoS·동작의 증명이 아니다.

```text
AUVSI ICD + IDL (source of truth; supplied separately)
  ├─ narrative, sections, operation tables, message tables, UML diagrams
  └─ native IDL field types, @key, @optional, inheritance
                 ↓ (cross-check)
contracts/services/<domain>/<name>.service.yaml
  ├─ metadata: 사용자 친화적인 서비스 이름과 설명
  ├─ spec: 목적, operations, topics, flows, 원본 표/설명/섹션
  ├─ provenance: ICD/IDL 원본과 검토 전사 기록, 버전·해시
  ├─ verification: 현재 **증거 상태**만 (자체 승인 불가)
  └─ x-*: UI/조직 로컬 확장; 공식 의미 변경 불가
                 ↓ (schema + semantic review + original IDL check)
             Validated immutable IR
                 ↓
 ServiceTraits / Component Generator / native DDS TypeBinding
                 ↓
  independently hosted Provider and Consumer over real DDS
```

## 첫 번째 실제 YAML

[GlobalPoseStatus.service.yaml](services/SA/GlobalPoseStatus.service.yaml)
— 6.1.6 및 6.1.6.1과 Operation Table 18, 메시지 Table 19의 정보로
구성한 **인간 검토용 문서 초안**. 다음 내용이 실제로 들어 있다.

- 목적: 글로벌 좌표계에서 차량 위치/자세를 보고
- `queryGlobalPose`: ICD에는 있으나 독립 Query DDS Topic은 만들지 않고 구독 처리
- `reportGlobalPose`: `GlobalPoseReportType` 발행
- `altitude`, `attitude`, `position` 등 11개 Table 19 속성의 타입·선택 여부·설명
- `UMAA::UMAAStatus` 기반 데이터 구조 참조
- 출처 표·조항·전사 저장소 리비전
- 미확정 DDS Topic 이름, 실제 `@key`, QoS, 원본 파일 해시: `null`

**중요:** 속성 설명은 ICD 내용의 검토용 *의역/주석*이며 필드 네이티브
TypeSupport 생성 입력이 아니다. 원본 IDL에서 타입, key, optional,
inheritance를 기계적으로 대조한 뒤에만 실제 DDS 코드 생성에 사용한다.

## 사용자 친화적인 기본 필드

| 블록 | 내용 | 편집자 |
| --- | --- | --- |
| `apiVersion`, `kind` | 형식 버전과 문서 유형; Kubernetes처럼 고정 | 시스템 |
| `metadata` | 이름, Namespace, 도메인, 한 줄 설명, labels | 작성자 |
| `spec.purpose` | ICD 목적/업무 의미 | ICD 원본에서 추출·검토 |
| `spec.operations` | 입출력·행위·연결 Topic·ICD 조항·설명 | ICD 원본에서 추출·검토 |
| `spec.topics` | Topic ID, IDL 타입/경로, Reader/Writer 역할, 표 속성 | ICD+IDL 근거 대조 |
| `spec.flows` | Report, Command/Response, Set/List, Generalization 조합 | 공통 패턴 엔진 연결 |
| `spec.documentation` | 표 번호, 원문 구조, 단락, UML 도표 출처, 규칙 | ICD 원본에서 추출·검토 |
| `provenance` | 버전, SHA256, 조항, 전사 자료 ID | 변조 방지 |
| `verification` | 검토 상태 표시; **생성 승인 권한 아님** | 독립 검증 결과 |
| `x-*` | 조직용 UI·문서화·분류 메타데이터 | 조직 플러그인 |

장문 ICD는 `documentation.blocks`에 다음처럼 섹션별로 추가한다.
이렇게 하면 서비스 YAML의 핵심은 짧고, 표와 규칙은 접을 수 있는
편집기 영역으로 표현할 수 있다.

```yaml
documentation:
  serviceSection: "6.1.6"
  operationsTable: 18
  messageTables:
    - number: 19
      topicRef: pose
  blocks:
    - clause: "6.1.6"
      kind: paragraph
      title: Service purpose
      body: Vehicle global position and orientation status.
    - clause: "6.1.6.1"
      kind: table
      title: GlobalPoseReportType attributes
      columns: [Name, Type, Optional, Description]
      rows:
        - [position, GeoPosition2D, "false", Geographic position]
        - [attitude, Orientation3DNEDType, "false", Orientation]
    - clause: "5.2"
      kind: figure
      title: Query-less subscription lifecycle
      assetRef: documentation/figures/report-flow.svg
```

`documentation.blocks`는 원본 기반 *설명/표/그림의 데이터 구조*를
보존할 수 있지만, 이 예시의 Figure 경로는 **형식 설명용 가상 경로**이며
현재 저장소에 원본 그림 파일이 있다는 뜻은 아니다. 저작권·배포 허용
범위가 확인되기 전에는 원본 문서 전체를 저장소에 복제하지 않는다.
원문 자산은 별도 관리하며 각 블록에 조항과 해시를 연결한다.

## 두 번째 원문 보존 예시: Command/ACK/Status/ExecutionStatus

[GlobalVectorControl.service.yaml](services/MO/GlobalVectorControl.service.yaml)
— MO ICD 6.0 §6.1.5, Operations Table 22, 메시지 Tables 23~26.
현재 검토된 4개 Topic에 더해 기존 단순 JSON 카탈로그에서 누락되기
쉬운 `queryGlobalVectorCommandAck`, `queryGlobalVectorExecutionStatus`,
`cancelGlobalVectorCommand`, `reportGlobalVectorCancelCommandStatus`를
구분한다. Query는 구독으로 모델링하며, Cancel은 **기존 Command Topic
인스턴스의 Dispose**로 연결하되 새 Cancel Topic을 만들지 않는다.
Cancel에 따른 직접 DDS 결과 동작은 `dds-native`로 기록하고,
최종 상태 전이·Key·QoS는 완전한 ICD/IDL 대조 전까지 미확정이다.

Command/ACK/Status/ExecutionStatus의 기존 Type/Namespace는
`spec.topics`에 **각각 한 번씩** 선언한다. Table 26의 Command
필드 여섯 개 및 Table 25의 실행 상태 필드 세 개의 선택 여부·설명은
`attributes`에 기록하되, 원본 IDL과 검증되기 전엔 타입 코드 생성에
사용하지 않는다.

## 복합 패턴 — 단일 enum으로 서비스를 제한하지 않는다

Service는 복수의 `flows`를 정의할 수 있고, 그 Flow가 여러 Topic을
참조한다. 동일 Topic은 한 곳에만 선언하여 중복을 방지한다.

```yaml
# GlobalVectorControl (MO ICD, 형식 예시; 실제 객체 전체가 아님)
spec:
  operations:
    setGlobalVector:
      actor: consumer
      transport: command
      topicRef: command
      clause: "6.1.5.4"
      description: Issue global speed/direction/elevation control
    queryGlobalVectorCommandAck:
      actor: consumer
      transport: subscribe
      topicRef: ack
      clause: "6.1.5"
      description: Subscribe to the correlated command acknowledgment
    # cancelGlobalVectorCommand is specified in the ICD table;
    # DDS Dispose is not modeled as a new command Topic.
  flows:
    mainCommand:
      kind: command-response
      commandTopicRef: command
      ackTopicRef: ack
      statusTopicRef: status
      executionStatusTopicRef: executionStatus
```

복합 패턴 `large-set`/`large-list`는 `metadataTopicRef`,
`elementTopicRef`를 동시에 요구하고,
`generalization-specialization`은 `generalTopicRef` 및
`specializationTopicRefs`와 동일 ID·타임스탬프 검증 규칙을 요구한다.
형식 스키마는 이 참조를 보존하지만 실제 native DDS 의미 완성 여부는
별도의 프로토콜/TypeSupport 통합 검증으로 판단한다.

## Docker Compose 방식으로 확장하되 *규범적 부분은 불변*

1. **`x-<조직>` 확장 허용:** 미지의 일반 필드는 오류지만
   `x-rydolabs`나 `x-org.example` 같은 사용자 메타데이터는 보존.
   공식 IDL·Topic·Key·QoS·Flow를 변경하는 데 쓰면 안 된다.
2. **명시적 로컬 import:** `imports: [{path: profiles/report.yaml,
   sha256: "...", kind: ProtocolProfile}]`처럼 로컬 파일을 해시로 고정.
   현재 검증기는 경로·해시의 유효성만 확인하며, 내용의 합성은 **미구현**.
   원격 URL, 환경변수 치환, 상대경로 `../` 이동은 계약 해석에서 금지.
3. **YAML 앵커/merge 제한:** 처음에는 중복 키, YAML alias, `<<` merge를
   거부. 도구별 구현 차이로 규범적 필드가 몰래 덮어쓰이는 문제를 피한다.
   반복 항목은 계약 이름/참조로 재사용한다.
4. **프로필과 배포 설정 분리:** 서비스의 규범적 정의는 변경하지 않고
   Component Assembly와 Deployment 파일에서 인스턴스, DDS 도메인,
   벤더, 로그, 재시도/시뮬레이션 설정 등 *허용된 항목만* 조정한다.
5. **버전 규칙:** `kumaa.dev/v1alpha1`의 새 필드는
   `x-` 확장 또는 새 `apiVersion`에서 추가한다. 기존 계약을 묵시적으로
   재해석하지 않고 `contract diff` 후 명시적 승인·마이그레이션을 요구.

## 분리된 파일 타입 — 공식 ICD는 Component 이름을 알 필요가 없다

```yaml
# 구상 예시: Assembly와 Deployment는 별도 Schema를 가질 예정
apiVersion: kumaa.dev/v1alpha1
kind: ComponentAssembly
metadata: {name: navigation}
services:
  - serviceRef: UMAA::SA::GlobalPoseStatus
    role: consumer
  - serviceRef: UMAA::MO::GlobalVectorControl
    role: provider

---
apiVersion: kumaa.dev/v1alpha1
kind: Deployment
metadata: {name: navigation-local}
backend: cyclone
ddsDomain: 42
componentRef: navigation
```

**이는 별도 Kind의 미래 설계 예시**이며 이 ServiceICD 스키마/검증기로
실행 가능한 파일은 아니다. 실제 프로젝트의 기존 `umaa.component.json`
포맷은 별도 마이그레이션 결정 전까지 변경하지 않는다.

## 현재 검증 가능한 명령

```sh
python -m pip install -r contracts/requirements.txt
python -m contracts.validate contracts/services/SA/GlobalPoseStatus.service.yaml
python -m unittest discover -s contracts/tests -p 'test_*.py' -v
```

성공 시 반환할 상태는 **`STRUCTURAL_VALID_NOT_WIRE_APPROVED`**.
상태가 `wire-tested`라는 사용자의 YAML 글자나 `x-` 플러그인 값은
공식 UMAA 통신 바인딩 승인을 만들어내지 않는다.

향후 고려하는 CLI 경험 (아직 **미구현**):

```text
kumaa icd init --service UMAA::SA::GlobalPoseStatus
kumaa icd lint contracts/services/SA/GlobalPoseStatus.service.yaml
kumaa icd explain ... --section 6.1.6.1
kumaa icd diff --from 6.0 --to local
kumaa icd render ... --format md
kumaa generate --assembly navigation.component.yaml --backend cyclone
```

## 인수 게이트

- 원본 ICD Operation 요청/응답 표의 Query/Cancel을 포함해 모든
  동작이 빠짐없이 추적 가능해야 한다. Topic 미사용 DDS 행위에 가짜
  Topic을 만들지 않는다.
- 필드별 타입·Key·Optional·상속·컬렉션은 실제 공식 IDL과 일치해야
  하고, 문서 설명은 ICD 조항·표·그림 ID로 역추적 가능해야 한다.
- YAML 구조와 참조는 오프라인 검사 가능하며 모든 unknown 핵심 키는
  거부. 모호한 QoS/profile default를 공식 의미로 허용하지 않는다.
- 서비스 YAML과 별도의 Component/Deployment Overlay를 합친 뒤에도
  원래 공식 IDL Topic/Key/필드/Flow가 바뀌지 않아야 한다.
- **독립 프로세스** Provider/Consumer의 실제 DDS DATA/ACK/STATUS/
  DISPOSE 및 복합 Collection/Generalization 전송이 성공하기 전에는
  어떤 YAML도 공식 Wire-ready 상태를 주장할 수 없다.

**다음 구현 단계:** v1alpha1 리뷰 후 전 53개 검토 Service의 "초안"
생성을 자동화하되, 원본 ICD 문서의 Operation/Message/Flow 표를
별도 추출해 빠진 Query·Cancel·필드 설명을 메운다. 원본 IDL의
해시·타입·Key를 교차 검증하는 Validator는 별도로 구축하고,
그 뒤 `GlobalPoseStatus` Provider/Consumer 수직 통신을 완성한다.
