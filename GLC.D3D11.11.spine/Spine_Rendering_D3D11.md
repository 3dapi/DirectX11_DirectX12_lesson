# Spine Rendering(Direct3D 11)

## ✅ 버퍼 생성 및 업데이트

### 정점/인덱스 버퍼 생성
```cpp
D3D11_BUFFER_DESC bd = {};
bd.Usage = D3D11_USAGE_DYNAMIC;  // <-Map 사용: Map 사용 불가: D3D11_USAGE_DEFAULT
bd.ByteWidth = sizeof(XMFLOAT2) * countVtx;
bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;  // <-Map 사용: Map 사용 불가: 0

device->CreateBuffer(&bd, nullptr, &vbvPos); // Map 대신 UpdateSubresource 사용
```

### 데이터 업데이트
```cpp
// D3D11_USAGE_DEFAULT 를 사용해서 Map 사용이 불가능할 때
context->UpdateSubresource(vbvPos.Get(), 0, nullptr, vertices, 0, 0);
```

```cpp
// D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE 를 사용해서 Map 사용이 가능
D3D11_MAPPED_SUBRESOURCE mapped = {};
hr = d3dContext->Map(buf->vbvPos, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
if(FAILED(hr))
    return hr;
float* ptrPos = reinterpret_cast<float*>(mapped.pData);
attachment->computeWorldVertices(*slot, 0, meshAttachment->getWorldVerticesLength(), ptrPos, 0, 2);
d3dContext->Unmap(buf->vbvPos, 0);
```
---

## ✅ Blend 상태 설정 (Alpha blending)
```cpp
D3D11_BLEND_DESC blendDesc = {};
blendDesc.RenderTarget[0].BlendEnable = TRUE;
blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

device->CreateBlendState(&blendDesc, &m_rsAlpha);
context->OMSetBlendState(m_rsAlpha.Get(), nullptr, 0xFFFFFFFF);
```

---

## ✅ DepthStencil 상태 설정

### Z-Write OFF (DepthTest 켠 상태)
```cpp
D3D11_DEPTH_STENCIL_DESC dss = {};
dss.DepthEnable = TRUE;
dss.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
dss.DepthFunc = D3D11_COMPARISON_LESS;

device->CreateDepthStencilState(&dss, &m_dssZWriteOff);
context->OMSetDepthStencilState(m_dssZWriteOff.Get(), 0);
```

### 완전한 DepthTest OFF
```cpp
D3D11_DEPTH_STENCIL_DESC dss = {};
dss.DepthEnable = FALSE;

device->CreateDepthStencilState(&dss, &m_dssNoDepth);
context->OMSetDepthStencilState(m_dssNoDepth.Get(), 0);
```

---

## ✅ Input Layout
```cpp
vector<D3D11_INPUT_ELEMENT_DESC> layout = {
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

device->CreateInputLayout(layout.data(), (UINT)layout.size(),
    vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);

context->IASetInputLayout(m_inputLayout.Get());
```

---

## ✅ Spine 렌더 순서

1. `Skeleton::getDrawOrder()` 순회
2. `RegionAttachment` 또는 `MeshAttachment` 추출
3. `computeWorldVertices()`로 위치 계산
4. UV, 색상, 인덱스 데이터 생성
5. GPU 버퍼로 업로드 (UpdateSubresource)
6. 다음 순서로 렌더링:
   ```cpp
   OMSetBlendState(...)
   OMSetDepthStencilState(...)
   IASetVertexBuffers(...)
   IASetIndexBuffer(...)
   DrawIndexed(...)
   ```
```cpp
//ex)
 SceneSpine::Render()

	// 1. Update constant value
	d3dContext->UpdateSubresource(m_cnstMVP, 0, {}, &m_mtMVP, 0, 0);

	// 2. set the constant buffer
	d3dContext->VSSetConstantBuffers(0, 1, &m_cnstMVP);

	float blendFactor[4] = {0, 0, 0, 0}; // optional
	UINT sampleMask = 0xffffffff;
	d3dContext->OMSetBlendState       (m_rsAlpha, blendFactor, sampleMask);
	d3dContext->OMSetDepthStencilState(m_rsZwrite, 0);
	d3dContext->OMSetDepthStencilState(m_rsZtest, 0);

	// 3. set vertex shader
	d3dContext->VSSetShader(m_shaderVtx, {}, 0);

	// 4. set the input layout
	d3dContext->IASetInputLayout(m_vtxLayout);

	// 5. set the pixel shader
	d3dContext->PSSetShader(m_shaderPxl, {}, 0);

	// 6. set the texture
	d3dContext->PSSetShaderResources(0, 1, &m_srvTexture);

	// 7. primitive topology
	d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 8. set the sampler state
	d3dContext->PSSetSamplers(0, 1, &m_sampLinear);

	// draw
	for(int i=0; i<m_drawCount; ++i)
	{
		auto& buf = m_drawBuf[i];

		UINT strides[] = {sizeof(XMFLOAT2), sizeof(uint32_t), sizeof(XMFLOAT2)};
		UINT offsets[] = {0, 0, 0};
		//set the vertex buffer to context.
		ID3D11Buffer* vbs[] = { buf.vbvPos, buf.vbvDif, buf.vbvTex };
		d3dContext->IASetVertexBuffers(0, 3, vbs, strides, offsets);
		// set the index buffer to context.
		d3dContext->IASetIndexBuffer(buf.ibv, DXGI_FORMAT_R16_UINT, 0);

		// draw
		d3dContext->DrawIndexed(buf.numIb, 0, 0);
	}
```

---

## ✅ 기타 픽셀 셰이더 디버깅

```hlsl
float4 main_ps(PS_IN v) : SV_Target0
{
    return float4(1,1,1,1); // 흰색 출력 확인용
}
```

---

## ✅ 기타

- 정점 구조체의 stride, 슬롯 순서 반드시 일치해야 함
- SRV 바인딩 누락 시 캐릭터 출력 안됨
- DrawIndexed() 전후 상태 확인 필수

---

## 🔚 참고

- `Map()` 사용하고 싶다면 `D3D11_USAGE_DYNAMIC` + `CPUAccessFlags = WRITE`
- 하지만 Spine처럼 매 프레임 갱신될 경우 `UpdateSubresource()` 방식이 성능상 더 유리함
