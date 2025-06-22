#include <any>
#include <memory>
#include <Windows.h>
#include <d3d12.h>
#include <DirectxColors.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <wrl.h>
#include <shellapi.h>


#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"


const unsigned ConstBufMVP::ALIGNED_SIZE = (sizeof(ConstBufMVP) + 255) & ~255;


struct Vertex
{
	XMFLOAT3 p;
	uint8_t d[4];
};

MainApp::MainApp()
{
}

MainApp::~MainApp()
{
	Destroy();
}

int MainApp::Init()
{
	HRESULT hr = S_OK;
	auto d3dDevice        = std::any_cast<ID3D12Device*			>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto d3dRootSignature = std::any_cast<ID3D12RootSignature*	>(IG2GraphicsD3D::getInstance()->GetRootSignature());
	{
		ComPtr<ID3DBlob> shaderVtx{}, shaderPxl{};
		hr = G2::DXCompileShaderFromFile("assets/simple.hlsl", "main_vs", "vs_5_0", &shaderVtx);
		if (FAILED(hr))
			return hr;
		hr = G2::DXCompileShaderFromFile("assets/simple.hlsl", "main_ps", "ps_5_0", &shaderPxl);
		if (FAILED(hr))
			return hr;


		// 1. Descriptor Range 정의
		D3D12_DESCRIPTOR_RANGE descriptorRange = {};
		descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;          // Constant Buffer View
		descriptorRange.NumDescriptors = 1;                                   // CBV 1개
		descriptorRange.BaseShaderRegister = 0;                               // b0
		descriptorRange.RegisterSpace = 0;
		descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// 2. Root Parameter 설정 (Descriptor Table로 구성)
		D3D12_ROOT_PARAMETER rootParameter = {};
		rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;      // 또는 ALL
		rootParameter.DescriptorTable.NumDescriptorRanges = 1;
		rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;

		// 3. Root Signature Description 정의
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = 1;
		rootSignatureDesc.pParameters = &rootParameter;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		// 4. Root Signature 직렬화 및 생성
		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			}
			return hr;
		}

		hr = d3dDevice->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature)
		);

		if (FAILED(hr)) {
			return hr;
		}


		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0+sizeof(XMFLOAT3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_RASTERIZER_DESC rasterDesc = {};
			rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
			rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
			rasterDesc.FrontCounterClockwise = FALSE;
			rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
			rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
			rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			rasterDesc.DepthClipEnable = TRUE;
			rasterDesc.MultisampleEnable = FALSE;
			rasterDesc.AntialiasedLineEnable = FALSE;
			rasterDesc.ForcedSampleCount = 0;
			rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		D3D12_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;
			const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
				FALSE, FALSE,
				D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
				D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
				D3D12_LOGIC_OP_NOOP,
				D3D12_COLOR_WRITE_ENABLE_ALL
			};
			for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
			}
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
			psoDesc.pRootSignature = m_rootSignature.Get();
			psoDesc.BlendState = blendDesc;
			psoDesc.RasterizerState = rasterDesc;
			psoDesc.VS.pShaderBytecode = shaderVtx.Get()->GetBufferPointer();
			psoDesc.VS.BytecodeLength = shaderVtx.Get()->GetBufferSize();
			psoDesc.PS.pShaderBytecode = shaderPxl.Get()->GetBufferPointer();
			psoDesc.PS.BytecodeLength = shaderPxl.Get()->GetBufferSize();
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count = 1;

		hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
		if (FAILED(hr))
			return hr;
	}

	// 4. Create the vertex buffer.
	m_bufVtxCount = 8;

	D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

	{
		D3D12_RESOURCE_DESC bufDesc = {};
			bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufDesc.Alignment = 0;
			bufDesc.Width = m_bufVtxCount * sizeof(Vertex);
			bufDesc.Height = 1;
			bufDesc.DepthOrArraySize = 1;
			bufDesc.MipLevels = 1;
			bufDesc.Format = DXGI_FORMAT_UNKNOWN;
			bufDesc.SampleDesc.Count = 1;
			bufDesc.SampleDesc.Quality = 0;
			bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		hr = d3dDevice->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&bufDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_rscVtx));

		// Copy the triangle data to the vertex buffer.
		UINT8* pBeginData{};
		D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
		hr = m_rscVtx->Map(0, &readRange, reinterpret_cast<void**>(&pBeginData));
		if (FAILED(hr))
			return hr;
		Vertex* pCur = reinterpret_cast<Vertex*>(pBeginData);
		*pCur++ = { { -1.0f,  1.0f, -1.0f }, {   0,   0, 255, 255 } };
		*pCur++ = { {  1.0f,  1.0f, -1.0f }, {   0, 255,   0, 255 } };
		*pCur++ = { {  1.0f,  1.0f,  1.0f }, {   0, 255, 255, 255 } };
		*pCur++ = { { -1.0f,  1.0f,  1.0f }, { 255,   0,   0, 255 } };
		*pCur++ = { { -1.0f, -1.0f, -1.0f }, { 255,   0, 255, 255 } };
		*pCur++ = { {  1.0f, -1.0f, -1.0f }, { 255, 255,   0, 255 } };
		*pCur++ = { {  1.0f, -1.0f,  1.0f }, { 255, 255, 255, 255 } };
		*pCur++ = { { -1.0f, -1.0f,  1.0f }, {  70,  70,  70, 255 } };

		// Initialize the vertex buffer view.
		m_viewVtx.BufferLocation = m_rscVtx->GetGPUVirtualAddress();
		m_viewVtx.StrideInBytes = sizeof(Vertex);
		m_viewVtx.SizeInBytes = m_viewVtx.StrideInBytes * m_bufVtxCount;
	}

	//5. create index buffer
	uint16_t indices[] =
	{
		3, 1, 0,  2, 1, 3,
		0, 5, 4,  1, 5, 0,
		3, 4, 7,  0, 4, 3,
		1, 6, 5,  2, 6, 1,
		2, 7, 6,  3, 7, 2,
		6, 4, 5,  7, 4, 6,
	};
	m_bufIdxCount = sizeof(indices) / sizeof(indices[0]);
	{
		D3D12_RESOURCE_DESC bufDesc = {};
			bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufDesc.Alignment = 0;
			bufDesc.Width = sizeof(uint16_t) * m_bufIdxCount;
			bufDesc.Height = 1;
			bufDesc.DepthOrArraySize = 1;
			bufDesc.MipLevels = 1;
			bufDesc.Format = DXGI_FORMAT_UNKNOWN;
			bufDesc.SampleDesc.Count = 1;
			bufDesc.SampleDesc.Quality = 0;
			bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		hr = d3dDevice->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&bufDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_rscIdx));

		// Copy the triangle data to the vertex buffer.
		uint8_t* pBeginData{};
		D3D12_RANGE readRange{ 0, 0 };
		hr = m_rscIdx->Map(0, &readRange, reinterpret_cast<void**>(&pBeginData));
		if (FAILED(hr))
			return hr;
		uint16_t* pCur = reinterpret_cast<uint16_t*>(pBeginData);
		memcpy(pCur, indices, sizeof(uint16_t) * m_bufIdxCount);

		m_viewIdx.BufferLocation = m_rscIdx->GetGPUVirtualAddress();
		m_viewIdx.SizeInBytes = sizeof(uint16_t) * m_bufIdxCount;
		m_viewIdx.Format = DXGI_FORMAT_R16_UINT;
	}


	// 6. Create the constant buffer
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = FRAME_BUFFER_COUNT;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		hr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvHeap));
		if (FAILED(hr))
			return hr;
	}

	heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

	// 공통 리소스 설명
	D3D12_RESOURCE_DESC bufDesc = {};
		bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufDesc.Alignment = 0;
		bufDesc.Width = FRAME_BUFFER_COUNT * ConstBufMVP::ALIGNED_SIZE;
		bufDesc.Height = 1;
		bufDesc.DepthOrArraySize = 1;
		bufDesc.MipLevels = 1;
		bufDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufDesc.SampleDesc.Count = 1;
		bufDesc.SampleDesc.Quality = 0;
		bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


	// world matrix
	hr = d3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_cnstMVP));
	if (FAILED(hr))
		return hr;

	UINT d3dDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	// Create constant buffer views to access the upload buffer.
	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_cnstMVP->GetGPUVirtualAddress();
	D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
	for (int n = 0; n < FRAME_BUFFER_COUNT; n++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
		desc.BufferLocation = cbvGpuAddress;
		desc.SizeInBytes = ConstBufMVP::ALIGNED_SIZE;
		d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);
		cbvGpuAddress += desc.SizeInBytes;
		cbvCpuHandle.ptr = cbvCpuHandle.ptr + d3dDescriptorSize;
	}


	// Mapping the constant buffers.
	uint8_t* pBeginData{};
	hr = m_cnstMVP->Map(0, nullptr, reinterpret_cast<void**>(&pBeginData));
	if (FAILED(hr))
		return hr;
	m_csnstPtrMVP = reinterpret_cast<uint8_t*>(pBeginData);

	// 6. setup the world, view, projection matrix
	// Initialize the world matrix
	m_mtMVP.m = XMMatrixIdentity();

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_mtMVP.v = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	auto screeSize = std::any_cast<::SIZE*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_CMD::ATTRIB_SCREEN_SIZE));
	m_mtMVP.p = XMMatrixPerspectiveFovRH(XM_PIDIV2, screeSize->cx / (FLOAT)screeSize->cy, 0.01f, 100.0f);

	return S_OK;
}

int MainApp::Destroy()
{
	m_cnstMVP->Unmap(0, nullptr);
	m_pipelineState.Reset();
	return S_OK;
}

int MainApp::Update()
{
	// Update our time
	static float t = 0.0f;

	static ULONGLONG timeStart = 0;
	ULONGLONG timeCur = GetTickCount64();
	if (timeStart == 0)
		timeStart = timeCur;
	t = (timeCur - timeStart) / 1000.0f;

	// Animate the cube
	//
	m_mtMVP.m = XMMatrixRotationY(t);

	// 각 constant buffer에 복사

	auto currentFrameIndex = IG2GraphicsD3D::getInstance()->GetCurrentFrameIndex();
	uint8_t* dest = m_csnstPtrMVP + (currentFrameIndex * ConstBufMVP::ALIGNED_SIZE);
	memcpy(dest, &m_mtMVP, sizeof(ConstBufMVP));

	return S_OK;
}

int MainApp::Render()
{
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto d3dCommandList = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	auto d3dRootSignature = m_rootSignature.Get();


	// Set the graphics root signature and descriptor heaps to be used by this frame.
	d3dCommandList->SetGraphicsRootSignature(m_rootSignature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
	d3dCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Bind the current frame's constant buffer to the pipeline.
	UINT d3dDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	auto currentFrameIndex = IG2GraphicsD3D::getInstance()->GetCurrentFrameIndex();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
	gpuHandle.ptr = m_cbvHeap->GetGPUDescriptorHandleForHeapStart().ptr + currentFrameIndex * d3dDescriptorSize;
	d3dCommandList->SetGraphicsRootDescriptorTable(0, gpuHandle);

	// 4. Set input layout related bindings
	d3dCommandList->IASetVertexBuffers(0, 1, &m_viewVtx);
	d3dCommandList->IASetIndexBuffer(&m_viewIdx);
	d3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 5. Draw
	d3dCommandList->DrawIndexedInstanced(m_bufIdxCount, 1, 0, 0, 0);


	return S_OK;
}

