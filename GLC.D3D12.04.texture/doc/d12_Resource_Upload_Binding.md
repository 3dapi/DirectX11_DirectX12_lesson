
# D3D12 Resource Upload & Binding 흐름 정리

## 1. Vertex / Index Buffer

- CPU 메모리 → GPU Default Heap으로 복사
- 사용 함수:
  - `UpdateSubresources`
  - `ResourceBarrier`로 `COPY_DEST` → `VERTEX_AND_CONSTANT_BUFFER` 상태 전환
- 복사 후에 `GetGPUVirtualAddress()` 사용하여 `D3D12_VERTEX_BUFFER_VIEW`, `D3D12_INDEX_BUFFER_VIEW` 설정

```cpp
m_viewVtx.BufferLocation = m_rscVtx->GetGPUVirtualAddress();
m_viewIdx.BufferLocation = m_rscIdx->GetGPUVirtualAddress();
```

## 2. Constant Buffer (CBV)

- `UploadHeap`에 생성, `Map()`을 통해 CPU에서 직접 접근 가능
- `CreateConstantBufferView()`로 디스크립터 힙에 프레임 수만큼 등록
- 프레임별 오프셋 계산 후 `memcpy()` 사용

```cpp
memcpy(mappedPtr + currentFrameIndex * alignedSize, &matrix, sizeof(matrix));
```

## 3. Texture (SRV)

- `ResourceUploadBatch`로 비동기 업로드 요청
- `End(queue)` → `uploadOp.wait()`으로 업로드 완료 대기
- 완료 후 `CreateShaderResourceView()` 호출로 디스크립터 힙에 등록

```cpp
CreateWICTextureFromFile(..., &m_textureXlogo);
device->CreateShaderResourceView(m_textureXlogo.Get(), &srvDesc, cpuHandle);
```

## 4. Descriptor Heap

- 타입: `CBV_SRV_UAV`, 플래그: `SHADER_VISIBLE`
- 크기: `FRAME_BUFFER_COUNT * CBV + SRV 수`
- `GetDescriptorHandleIncrementSize()`는 디바이스 생성 후 아무 때나 호출 가능

```cpp
UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
```

## 5. Command List 마무리

모든 리소스 설정 후 실행

```cpp
commandList->Close();
commandQueue->ExecuteCommandLists(...);
WaitForGpu();
```

---

## 🔎 메모

- 상수 버퍼는 힙 필요 + Map 사용
- 버텍스/인덱스는 힙 없이 직접 default heap 사용 (업로더 거쳐 복사)
- 텍스처는 `SRV`로 등록 필요 → `DescriptorHeap` 필요
