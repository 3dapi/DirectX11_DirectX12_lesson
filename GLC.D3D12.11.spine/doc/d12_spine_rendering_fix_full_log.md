# Spine Rendering Fix Log - D3D12

**날짜**: 2025-07-04 06:53:01  
**렌더링 API**: Direct3D 12  
**목표**: Spine 애니메이션 텍스처 표시 문제 및 알파 블렌딩, Z-write/Z-test 설정 문제 해결

---

## 진행 내용 요약

### 1. 텍스처 및 셰이더 정상 동작 확인

- 텍스처 PNG 문제 없음.
- 픽셀 셰이더에서 `return float4(1,1,1,1)` 시 정상적으로 흰색 출력됨.
- UV를 출력(`return float4(v.t, 0, 1);`)하여 텍스처 좌표 확인, 정상 표시.

---

### 2. 정점 버퍼(DRAW_BUFFER) 전략

- Spine 애니메이션 구조 특성상 버텍스 수가 애니메이션에 따라 달라짐.
- 매 프레임마다 버퍼를 새로 생성하지 않도록 최대 크기 기준으로 한번 생성해두고 재사용.

```cpp
// 최대 버퍼 길이 찾기
	size_t maxVertexCount = 0;
	size_t maxIndexCount = 0;
	auto drawOrder = m_spineSkeleton->getDrawOrder();
	for(size_t i = 0; i < drawOrder.size(); ++i)
	{
		spine::Slot* slot = drawOrder[i];
		spine::Attachment* attachment = slot->getAttachment();
		if(!attachment)
			continue;
		if(attachment->getRTTI().isExactly(spine::MeshAttachment::rtti))
		{
			auto* mesh = static_cast<spine::MeshAttachment*>(attachment);
			size_t vtxCount = mesh->getWorldVerticesLength() / 2;
			if(vtxCount > maxVertexCount)
				maxVertexCount = vtxCount;

			size_t indexCount = mesh->getTriangles().size();
			if(indexCount > maxIndexCount)
				maxIndexCount = indexCount;
		}
	}
	// buffer 최댓값으로 설정.
	m_maxVtxCount = UINT( (maxVertexCount>8) ? maxVertexCount : 8 );
	m_maxIdxCount = UINT( (maxIndexCount>8) ? maxIndexCount : 8 );

   //...
	const UINT64 widthVertex = m_maxVtxCount * sizeof(XMFLOAT2);
	const UINT64 widthIndex  = m_maxIdxCount * sizeof(uint16_t);
	const UINT64 widthVertex = m_maxVtxCount * sizeof(XMFLOAT2);
	const UINT64 widthIndex  = m_maxIdxCount * sizeof(uint16_t);
	CD3DX12_HEAP_PROPERTIES heapPropsGPU	(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_HEAP_PROPERTIES heapPropsUpload	(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC   vtxBufDesc		= CD3DX12_RESOURCE_DESC::Buffer(widthVertex);
	CD3DX12_RESOURCE_DESC	idxBufDesc		= CD3DX12_RESOURCE_DESC::Buffer(widthIndex);
	device->CreateCommittedResource(...);
```

- 업로드 시 매번 버퍼 전체를 Clear 하거나 필요한 부분만 업데이트.

---

### 3. 버퍼 바인딩

```cpp
buf.vbvPos = { buf.rscPosGPU->GetGPUVirtualAddress(), vertexCount * sizeof(XMFLOAT2), sizeof(XMFLOAT2) };
buf.vbvDif = { buf.rscDifGPU->GetGPUVirtualAddress(), vertexCount * sizeof(uint32_t), sizeof(uint32_t) };
buf.vbvTex = { buf.rscTexGPU->GetGPUVirtualAddress(), vertexCount * sizeof(XMFLOAT2), sizeof(XMFLOAT2) };
cmdList->IASetVertexBuffers(0, 1, &buf.vbvPos);
cmdList->IASetVertexBuffers(1, 1, &buf.vbvDif);
cmdList->IASetVertexBuffers(2, 1, &buf.vbvTex);
```

- `vbvDif`가 올바르게 초기화되지 않아 색상 오류 발생.
- `memset(ptr, 0xFF, vertexCount * sizeof(uint32_t));` 혹은 `for` 루프 모두 동작 가능.

---

### 4. Alpha Blending 및 Depth 설정

```cpp
psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

// 깊이 관련
psoDesc.DepthStencilState.DepthEnable = TRUE;
psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
```

- `DepthEnable = FALSE` 와 `DepthFunc = ALWAYS` 는 유사하지만 `TRUE + ALWAYS` 조합이 더 명확함.
- `DepthWriteMask = ZERO`로 Z-Write 끔.

---

### 5. 최종 렌더링 결과

- D3D11과 동일한 비주얼 결과 도달.
- Spine 애니메이션 텍스처 문제 해결, 알파 블렌딩 및 깊이 설정 완료.

---

### 6. 참고사항

- D3D12에서는 `D3D12_GRAPHICS_PIPELINE_STATE_DESC`로 모든 고정 기능 상태 설정.
- PSO 상태는 실행 중 변경 불가 → 다른 PSO를 미리 만들어둬야 함.
- Spine 텍스처 및 애니메이션 구조 이해가 중요 (RegionAttachment vs MeshAttachment).

