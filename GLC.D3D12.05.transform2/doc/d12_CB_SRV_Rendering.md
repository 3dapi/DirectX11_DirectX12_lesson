# DirectX 12 텍스처 및 Constant Buffer 렌더링 정리

## ✅ 주요 구조

### 📦 상수 버퍼(CBV)
- **3개의 행렬 (World, View, Projection)** 을 각각 개별 버퍼에 저장
- 각 행렬 버퍼는 `FRAME_BUFFER_COUNT` 만큼 프레임별로 복제되어 있음

```cpp
heapDesc.NumDescriptors = FRAME_BUFFER_COUNT * 3 + 1; // +1: SRV용
```

### 📦 Descriptor Heap 인덱싱
| 목적 | 시작 인덱스 | 총 개수 |
|------|-------------|---------|
| b0 (mtWld) | 0 | FRAME_BUFFER_COUNT |
| b1 (mtViw) | FRAME_BUFFER_COUNT | FRAME_BUFFER_COUNT |
| b2 (mtPrj) | FRAME_BUFFER_COUNT * 2 | FRAME_BUFFER_COUNT |
| SRV (t0)   | FRAME_BUFFER_COUNT * 3 | 1 |

---

## ⚙️ 루트 시그니처 설정

```cpp
CD3DX12_ROOT_PARAMETER rootParams[4];
rootParams[0].InitAsDescriptorTable(1, &cbvRange[0], D3D12_SHADER_VISIBILITY_VERTEX); // b0
rootParams[1].InitAsDescriptorTable(1, &cbvRange[1], D3D12_SHADER_VISIBILITY_VERTEX); // b1
rootParams[2].InitAsDescriptorTable(1, &cbvRange[2], D3D12_SHADER_VISIBILITY_VERTEX); // b2
rootParams[3].InitAsDescriptorTable(1, &srvRange   , D3D12_SHADER_VISIBILITY_PIXEL);  // t0
```

---

## 🖼️ 셰이더 코드

```hlsl
cbuffer MVP0 : register(b0) { matrix mtWld; }
cbuffer MVP1 : register(b1) { matrix mtViw; }
cbuffer MVP2 : register(b2) { matrix mtPrj; }
Texture2D gTexDif : register(t0);
SamplerState gSmpLinear : register(s0);
```

---

## 🧮 CBV/SRV 할당 및 바인딩

### CBV View 생성

```cpp
for (int n = 0; n < FRAME_BUFFER_COUNT; n++) {
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
    desc.BufferLocation = cbvGpuAddress;
    desc.SizeInBytes = G2::align256BufferSize(sizeof XMMATRIX);
    d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);
    cbvGpuAddress += desc.SizeInBytes;
    cbvCpuHandle.ptr += descriptorSize;
}
```

### SRV 생성 위치는 반드시 `FRAME_BUFFER_COUNT * 3`

```cpp
CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), FRAME_BUFFER_COUNT * 3, descriptorSize);
CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), FRAME_BUFFER_COUNT * 3, descriptorSize);
```

---

## 🖌️ Draw 시 바인딩

```cpp
CD3DX12_GPU_DESCRIPTOR_HANDLE handleWld(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), 0 * FRAME_BUFFER_COUNT + currentFrameIndex, descriptorSize);
CD3DX12_GPU_DESCRIPTOR_HANDLE handleViw(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), 1 * FRAME_BUFFER_COUNT + currentFrameIndex, descriptorSize);
CD3DX12_GPU_DESCRIPTOR_HANDLE handlePrj(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), 2 * FRAME_BUFFER_COUNT + currentFrameIndex, descriptorSize);

cmdList->SetGraphicsRootDescriptorTable(0, handleWld);
cmdList->SetGraphicsRootDescriptorTable(1, handleViw);
cmdList->SetGraphicsRootDescriptorTable(2, handlePrj);
cmdList->SetGraphicsRootDescriptorTable(3, m_srvHandle);
```

---

## ❗ 중요 체크포인트
- descriptor heap 인덱스 잘못 지정하면 **크래시 발생**
- **SRV 위치는 CBV 끝 다음 인덱스로 설정할 것**
- `CreateShaderResourceView()`와 `SetGraphicsRootDescriptorTable()` 인덱스 **일치해야 함**
- `Map()`으로 버퍼에 접근하고 `memcpy`로 행렬 쓰는 부분 확인 필요

---

작성일: 2025-07-03
