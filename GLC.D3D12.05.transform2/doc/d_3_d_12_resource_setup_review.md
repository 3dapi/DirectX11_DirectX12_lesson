# D3D12 자원 설정 구조 및 함수/주석 평가

## 1. 전체 자원 초기화 구조
Direct3D 12 자원 초기화는 다음과 같은 환상으로 구성되어 있고,
자원의 독립성/공용 가능 유물에 따라 분류됨:

1. `Init()`
   - 전체 초기화 환상 관리
2. `InitForDevice()`
   - 완전 독립 자원: 텍스쳐, 셔이더, 루트 시그\uub2c8체, 파이프라이닝
   - 디스크립터 힐 (CBV + SRV)
   - CBV 리소스 및 뷰 설정
   - SRV 디스크립터 설정
3. `SetupResource()`
   - 정점/인덱스 버퍼 생성 및 GPU 업로드
4. `InitConstValue()`
   - 초기 뷰/투영 헤치 설정

## 2. 함수 이름 평가 및 제안
- ✅ `Init()` → 전체 초기화로 적절
- ⚠️ `InitForDevice()` → `InitDeviceResources()` 또는 `InitSharedResources()` 권장
- ✅ `SetupResource()` → 객체 리소스 초기화에 적합
- ⚠️ `InitConstValue()` → `InitDefaultConstants()` 등으로 더 명확히 표현 가능

## 3. Render() 구성 평가
- 루트 시그\uub2c8체 → 파이프라이닝 → 토폴로지 → VTX/IDX → CBV/SRV → Draw 순서로 명확하게 정리됨
- `cbv0 ~ cbv2`에 `b0 ~ b2` 주석 추가하면 이해 쉽음
- 텍스쳐 SRV 히솔 설정도 명확하게 구현됨

## 4. 주석 및 종속성 설명
- 주석으로 자원 종속성(완전 독립, 루트 시그\uub2c8체 종속, 디스크립터 힐 종속 등)을 명확히 구분함
- "셔이더와 일치하는 것만" → "셔이더와 root signature가 일치해야 함"으로 변경 제안
- "상수 + 텍스쳐 버퍼 heap" → "상수 버퍼 + 텍스쳐 SRV용 디스크립터 힐"으로 구체화

## 5. 구조체 개선 제안
- `CBV_RSC::len` → 256 바이트 정렬된 크기임을 명시 주석 추가 권장
- `rpl` → 역할이 명확히 드러내도록 주석 추가 (예: per-frame world/view/proj buffer list)

