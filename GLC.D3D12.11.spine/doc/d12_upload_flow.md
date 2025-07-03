# D3D12 리소스 업로드 흐름 및 안전한 처리 방식

Direct3D 12에서 GPU 리소스를 초기화할 때는 반드시 GPU가 리소스 복사를 완료하기 전까지 관련된 리소스를 유지해야 하며, 그 동안의 비동기 처리 흐름을 고려해야 합니다.

---

## 문제 요약: GPU는 비동기로 동작

- `UpdateSubresources()`를 호출하면 실제 복사는 GPU 명령 큐에 기록만 됨.
- 호출 후에 업로드 리소스(`ID3D12Resource` with `UPLOAD` heap)가 해제되면 GPU가 해당 메모리에 접근할 수 없음.
- 이로 인해 GPU는 유효한 리소스를 참조하지 못하고, 결과적으로 아무 것도 렌더링되지 않음.

---

## 안전한 업로드 패턴

```cpp
// 업로드 대상: Vertex / Index 등
Vertex vertices[] = { ... };
uint16_t indices[] = { ... };

// 리소스 선언
ComPtr<ID3D12Resource> vertexBufferGPU;
ComPtr<ID3D12Resource> vertexBufferUpload;

// 1. GPU/UPLOAD 리소스 생성
CD3DX12_HEAP_PROPERTIES heapPropsGPU(D3D12_HEAP_TYPE_DEFAULT);
CD3DX12_HEAP_PROPERTIES heapPropsUpload(D3D12_HEAP_TYPE_UPLOAD);
CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

d3dDevice->CreateCommittedResource(&heapPropsGPU, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&vertexBufferGPU));

d3dDevice->CreateCommittedResource(&heapPropsUpload, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBufferUpload));

// 2. 업로드 커맨드 기록
D3D12_SUBRESOURCE_DATA vertexData = {};
vertexData.pData = reinterpret_cast<const BYTE*>(vertices);
vertexData.RowPitch = vertexBufferSize;
vertexData.SlicePitch = vertexBufferSize;

UpdateSubresources(commandList, vertexBufferGPU.Get(), vertexBufferUpload.Get(), 0, 0, 1, &vertexData);

// 3. 상태 전환
auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertexBufferGPU.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
commandList->ResourceBarrier(1, &barrier);

// 4. 커맨드 큐 제출
commandList->Close();
commandQueue->ExecuteCommandLists(1, &commandList);

// 5. GPU 작업 완료 대기
WaitForGpu(); // Fence wait 또는 ResourceUploadBatch::End().wait()
```

---

## 핵심 요약

| 단계 | 설명 |
|------|------|
| **1. 리소스 생성** | GPU용(D3D12_HEAP_TYPE_DEFAULT), 업로드용(D3D12_HEAP_TYPE_UPLOAD) 모두 필요 |
| **2. 데이터 준비** | 정점/인덱스/텍스처 등을 `SUBRESOURCE_DATA`로 구성 |
| **3. `UpdateSubresources` 호출** | CPU에서 GPU 리소스로 복사하는 명령을 기록 (복사 아님) |
| **4. ResourceBarrier 적용** | 상태 전환 필수: `COPY_DEST → VERTEX_AND_CONSTANT_BUFFER` 등 |
| **5. 커맨드 제출 + GPU 대기** | GPU가 끝나기 전까지 관련 리소스는 유효해야 함 |

---

## 부록: 텍스처용 `ResourceUploadBatch` 예시

```cpp
DirectX::ResourceUploadBatch resourceUpload(device);
resourceUpload.Begin();

CreateWICTextureFromFile(device, resourceUpload, L"texture.png", &textureResource);

resourceUpload.End(commandQueue).wait(); // 업로드 완료까지 대기
```

---

## 결론

- GPU는 비동기. **업로드 리소스는 GPU가 끝낼 때까지 살아있어야 한다.**
- 함수 안에서 지역으로 선언된 업로드 버퍼라면 반드시 **GPU 완료 대기 후 함수 종료**
- 텍스처, 버텍스, 인덱스 모두 동일한 구조로 처리 가능

이 구조를 따라가면 D3D12 리소스 업로드에서 발생할 수 있는 "묵묵히 안 나오는 문제"를 방지할 수 있습니다.

