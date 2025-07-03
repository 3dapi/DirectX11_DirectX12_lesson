# D3D12 Texture Rendering Flow Summary

This document summarizes the complete flow for rendering a textured cube in Direct3D 12 using DirectXTK12 and shader registers `b0`, `t0`, and `s0`.

---

## 1. Vertex Structure and Input Layout

```cpp
struct Vertex {
    XMFLOAT3 p;   // POSITION
    union {
        uint8_t d[4];  // COLOR
        uint32_t c;
    };
    XMFLOAT2 t;   // TEXCOORD
};
```

### Input Layout
```cpp
const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM , 0, sizeof(XMFLOAT3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT   , 0, sizeof(XMFLOAT3) + sizeof(uint32_t), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};
```

---

## 2. Root Signature

```cpp
CD3DX12_DESCRIPTOR_RANGE cbvRange;
cbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0

CD3DX12_DESCRIPTOR_RANGE srvRange;
srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

CD3DX12_ROOT_PARAMETER rootParams[2];
rootParams[0].InitAsDescriptorTable(1, &cbvRange, D3D12_SHADER_VISIBILITY_VERTEX);
rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

CD3DX12_STATIC_SAMPLER_DESC staticSampler(0);

CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
rootSigDesc.Init(
    _countof(rootParams),
    rootParams,
    1, &staticSampler,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
);
```

---

## 3. Descriptor Heap

```cpp
D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
heapDesc.NumDescriptors = FRAME_BUFFER_COUNT + 1;
heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvHeap));
```

---

## 4. Constant Buffer View (CBV)

```cpp
D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_cnstMVP->GetGPUVirtualAddress();
D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
for (int n = 0; n < FRAME_BUFFER_COUNT; n++) {
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
    desc.BufferLocation = cbvGpuAddress;
    desc.SizeInBytes = ConstBufMVP::ALIGNED_SIZE;
    d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);
    cbvGpuAddress += desc.SizeInBytes;
    cbvCpuHandle.ptr += d3dDescriptorSize;
}
```

---

## 5. Texture Load and SRV (DirectXTK12)

```cpp
ResourceUploadBatch resourceBatch(d3dDevice);
resourceBatch.Begin();
CreateWICTextureFromFile(d3dDevice, resourceBatch, L"res_checker.png", &m_texChecker);
resourceBatch.End(cmdQueue).wait();

D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Format = m_texChecker->GetDesc().Format;
srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MipLevels = 1;

D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
srvCpuHandle.ptr += d3dDescriptorSize * FRAME_BUFFER_COUNT;
d3dDevice->CreateShaderResourceView(m_texChecker.Get(), &srvDesc, srvCpuHandle);

m_srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), FRAME_BUFFER_COUNT, d3dDescriptorSize);
```

---

## 6. Render Function Binding

```cpp
cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
cmdList->SetDescriptorHeaps(1, m_cbvHeap.GetAddressOf());

CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), currentFrameIndex, d3dDescriptorSize);
cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
cmdList->SetGraphicsRootDescriptorTable(1, m_srvHandle);
```

---

## 7. Shader Code (HLSL)

```hlsl
cbuffer MVP0 : register(b0) {
    matrix mtWld, mtViw, mtPrj;
}

Texture2D gTexDif : register(t0);
SamplerState gSmpLinear : register(s0);

struct VS_IN {
    float4 p : POSITION;
    float4 d : COLOR;
    float2 t : TEXCOORD0;
};

struct PS_IN {
    float4 p : SV_POSITION;
    float4 d : COLOR;
    float2 t : TEXCOORD;
};

PS_IN main_vs(VS_IN v) {
    PS_IN o = (PS_IN)0;
    o.p = mul(mtPrj, mul(mtViw, mul(mtWld, v.p)));
    o.d = v.d;
    o.t = v.t;
    return o;
}

float4 main_ps(PS_IN v) : SV_Target {
    return gTexDif.Sample(gSmpLinear, v.t) * v.d;
}
```

---

End of document.

