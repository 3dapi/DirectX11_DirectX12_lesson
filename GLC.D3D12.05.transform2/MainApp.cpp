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
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>

#include "d3dx12.h"
#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"
#include "D3DApp.h"
#include "GameTimer.h"
GameTimer	m_timer;

const static int ALIGNED_MATRIX_SIZE_B0 = G2::align256BufferSize(sizeof(XMMATRIX));
const static int ALIGNED_MATRIX_SIZE_B1 = G2::align256BufferSize(sizeof(XMMATRIX));
const static int ALIGNED_MATRIX_SIZE_B2 = G2::align256BufferSize(sizeof(XMMATRIX));


ConstHeap::~ConstHeap()
{
	if (dscCbvHeap) {
		dscCbvHeap->Release();
		dscCbvHeap = {};
		descHandle.ptr = {};
	}
	if (!cbvRpl.empty()) {
		for (size_t i = 0; i < cbvRpl.size(); ++i) {
			if (cbvRpl[i].rsc) {
				cbvRpl[i].rsc->Unmap(0, nullptr);
				cbvRpl[i].rsc->Release();
				cbvRpl[i].ptr = nullptr;
				cbvRpl[i].len = 0;
			}
		}
		cbvRpl.clear();
	}
	if (!srvTex.empty()) {
		for (size_t i = 0; i < srvTex.size(); ++i) {
			srvTex[i].ptr = {};
		}
		srvTex.clear();
	}
}

void ConstHeap::setupRpl(const vector<UINT>& rplSize)
{
	for (size_t i=0; i<rplSize.size(); ++i)
	{
		cbvRpl.push_back({ nullptr, nullptr, rplSize[i] });
	}
}

MainApp::MainApp()
{
}

MainApp::~MainApp()
{
	Destroy();
}

int MainApp::Init()
{
	m_timer.Reset();

	HRESULT hr = S_OK;
	hr = InitDeviceResource();
	if(FAILED(hr))
		return hr;
	hr = SetupResource();
	if (FAILED(hr))
		return hr;
	hr = InitDefaultConstant();
	if(FAILED(hr))
		return hr;

	return S_OK;
}

int MainApp::Destroy()
{
	if(!m_textureLst.empty())
	{
		for(size_t i=0;i< m_textureLst.size(); ++i)
		{
			m_textureLst[i]->Release();
		}
		m_textureLst.clear();
	}

	m_objCbv.clear();
	m_objValue.clear();
	m_cbvListSize.clear();

	m_rootSignature.Reset();
	m_pipelineState.Reset();
	m_vtxView = {};
	m_vtxView = {};
	m_vtxCount = 0;
	m_idxCount = 0;

	m_objCbv.clear();
	m_objValue.clear();

	return S_OK;
}

int MainApp::Update()
{
	m_timer.Tick();
	auto deltaTime = m_timer.DeltaTime();

	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto currentFrameIndex = *(std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));

	if(true)
	{
		m_angle += deltaTime ;
		auto tmWld = XMMatrixRotationY((float)m_angle);
		m_objValue[0].tmWld = tmWld;
		m_objValue[1].tmWld = XMMatrixScaling(0.5F, 0.5F, 0.5F) * XMMatrixRotationY(FLOAT(m_angle)*2.0F) * XMMatrixTranslation(-400,    0, 0);
		m_objValue[2].tmWld = XMMatrixScaling(0.7F, 0.7F, 0.7F) * XMMatrixRotationY(FLOAT(m_angle)*0.5F) * XMMatrixTranslation( 400,    0, 0);
		m_objValue[3].tmWld = XMMatrixScaling(0.4F, 0.4F, 0.4F) * XMMatrixRotationY(FLOAT(m_angle)*3.0F) * XMMatrixTranslation(   0, -450, 0);
		m_objValue[4].tmWld = XMMatrixScaling(0.3F, 0.3F, 0.3F) * XMMatrixRotationY(FLOAT(m_angle)*0.3F) * XMMatrixTranslation(   0,  350, 0);

		for(size_t i=0; i< m_objCbv.size(); ++i)
		{
			uint8_t* dest = {};
			auto& cbv = m_objCbv[i];
			dest = cbv.cbvRpl[0].ptr + (currentFrameIndex * cbv.cbvRpl[0].len);
			memcpy(dest, &m_objValue[i].tmWld, sizeof(XMMATRIX));

			dest = cbv.cbvRpl[1].ptr + (currentFrameIndex * cbv.cbvRpl[1].len);
			memcpy(dest, &m_objValue[i].tmViw, sizeof(XMMATRIX));

			dest = cbv.cbvRpl[2].ptr + (currentFrameIndex * cbv.cbvRpl[2].len);
			memcpy(dest, &m_objValue[i].tmPrj, sizeof(XMMATRIX));
		}
	}
	return S_OK;
}

int MainApp::Render()
{
	HRESULT hr = S_OK;
	auto d3dDevice         = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto cmdList           = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	UINT currentFrameIndex = *(std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));
	UINT descriptorSize    = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 1. 루트 시그너처
	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
	// 2. 파이프라인 연결
	cmdList->SetPipelineState(m_pipelineState.Get());
	// 3. 토폴로지
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 4. 버택스, 인덱스 버퍼
	cmdList->IASetVertexBuffers(0, 1, &m_vtxView);
	cmdList->IASetIndexBuffer(&m_idxView);

	for (size_t i=0; i<m_objCbv.size(); ++i)
	{
		auto& cbv = m_objCbv[i];

		// 5. 디스크립터 힙 설정
		auto& cbvHeap = cbv.dscCbvHeap;
		ID3D12DescriptorHeap* descriptorHeaps[] = { cbvHeap };
		cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		// 6. cbv 바인딩
		for(UINT kk=0; kk<(UINT)cbv.cbvRpl.size(); ++kk)
		{
			auto base = cbvHeap->GetGPUDescriptorHandleForHeapStart();
			auto gpuH = CD3DX12_GPU_DESCRIPTOR_HANDLE(base, FRAME_BUFFER_COUNT * kk, descriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(kk, gpuH);
		}

		// 7. SRV 바인딩
		UINT base = (UINT)cbv.cbvRpl.size();
		for(UINT kk=0; kk<(UINT)cbv.srvTex.size(); ++kk)
		{
			cmdList->SetGraphicsRootDescriptorTable(base + kk, cbv.srvTex[ kk ]);
		}
		
		// 8. draw
		cmdList->DrawIndexedInstanced(m_idxCount, 1, 0, 0, 0);
	}

	return S_OK;
}


int MainApp::InitDeviceResource()
{
	HRESULT hr = S_OK;
	auto d3dDevice      = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto commandAlloc   = std::any_cast<ID3D12CommandAllocator*>(IG2GraphicsD3D::getInstance()->GetCommandAllocator());
	auto commandList    = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// setup the const buffer desc buffer size
	// 0.
	m_cbvListSize = vector<UINT>{ ALIGNED_MATRIX_SIZE_B0, ALIGNED_MATRIX_SIZE_B1, ALIGNED_MATRIX_SIZE_B2 };

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 1. 완전 독립(공유가능) : 텍스처 생성 및 업로드 (CreateWICTextureFromFile + ResourceUploadBatch)
	{
		DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
		{
			ID3D12Resource* texture{};
			resourceUpload.Begin();
			hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, L"assets/res_checker.png", &texture);
			if (FAILED(hr))
				return hr;
			auto commandQueue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
			auto uploadOp = resourceUpload.End(commandQueue);
			uploadOp.wait();  // GPU 업로드 완료 대기
			m_textureLst.push_back(texture);
		}
	}
	{
		DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
		{
			ID3D12Resource* texture{};
			resourceUpload.Begin();
			hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, L"assets/xlogo.png", &texture);
			if (FAILED(hr))
				return hr;
			auto commandQueue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
			auto uploadOp = resourceUpload.End(commandQueue);
			uploadOp.wait();  // GPU 업로드 완료 대기
			m_textureLst.push_back(texture);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 2. 완전 독립(공유가능) : 셰이더 자원
	// compile shader
	ComPtr<ID3DBlob> shaderVtx{}, shaderPxl{};
	{
		HRESULT hr = S_OK;
		hr = G2::DXCompileShaderFromFile("Shaders/simple.hlsl", "vs_5_0", "main_vs", &shaderVtx);
		if (FAILED(hr))
			return hr;
		hr = G2::DXCompileShaderFromFile("Shaders/simple.hlsl", "ps_5_0", "main_ps", &shaderPxl);
		if (FAILED(hr))
			return hr;
	}
	// Setup the input element decription.
	static const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM , 0, sizeof(XMFLOAT3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT   , 0, sizeof(XMFLOAT3) + sizeof(uint32_t), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 3. 완전 독립(공유가능) : 셰이더와 root signature가 일치.
	// Create a root signature with a single constant buffer slot.
	{
		// sampler register 갯수는 상관 없음.
		static vector<CD3DX12_STATIC_SAMPLER_DESC> staticSampler =
		{
			{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 4, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 5, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 6, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 7, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 8, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 9, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{10, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{11, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
		};

		// 루트 시그너처 생성: 현재는 상수 버퍼 개수(m_cbvListSize)와 텍스처 수(m_textureLst)에 의존
		// 최적 방법은 셰이더에서 실제 사용 개수를 분석하여 자동화하는 것.
		UINT sizeCBV = (UINT)m_cbvListSize.size();
		UINT sizeSRV = (UINT)m_textureLst.size();
		UINT sizeRootParam = sizeCBV + sizeSRV;

		vector<CD3DX12_DESCRIPTOR_RANGE> descRange(sizeRootParam);
		vector<CD3DX12_ROOT_PARAMETER>   rootParam(sizeRootParam);

		// CBV: b0 ~
		for (UINT i = 0; i < sizeCBV; ++i) {
			descRange[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, i); // b0, b1, b2, ...
			rootParam[i].InitAsDescriptorTable(1, &descRange[i], D3D12_SHADER_VISIBILITY_VERTEX);
		}

		// SRV: t0 ~
		for (UINT i = 0; i < sizeSRV; ++i) {
			UINT idx = sizeCBV + i;
			descRange[idx].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i); // t0, t1, ...
			rootParam[idx].InitAsDescriptorTable(1, &descRange[idx], D3D12_SHADER_VISIBILITY_PIXEL);
		}

		// 루트 시그너처 생성
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init(
			static_cast<UINT>(rootParam.size()),
			rootParam.data(),
			(UINT)staticSampler.size(),
			&staticSampler[0],
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		);


		ComPtr<ID3DBlob> pSignature{}, pError{};
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
			return hr;
		hr = d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
		if (FAILED(hr))
			return hr;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 4. 부분 독립(공유가능) : 파이프라인 스테이트: 루트 시그너처 필수:
	// Create the pipeline state once the shaders are loaded.
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(shaderVtx.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(shaderPxl.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = *std::any_cast<DXGI_FORMAT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_RENDER_TARGET_FORAT));
		psoDesc.DSVFormat = *std::any_cast<DXGI_FORMAT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_DEPTH_STENCIL_FORAT));
		psoDesc.SampleDesc.Count = 1;
		hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
		if (FAILED(hr))
			return hr;
	};

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	UINT numDescriptor = FRAME_BUFFER_COUNT * (UINT)m_cbvListSize.size() + (UINT)m_textureLst.size();
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);

	// implement to object list
	for (size_t i=0; i<m_objCbv.size(); ++i)
	{
		auto& cbv = m_objCbv[i];
		cbv.setupRpl(m_cbvListSize);			// constant buffer register
		cbv.srvTex.resize(m_textureLst.size(), {});

		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 5. Rendering Object 종속: 상수 버퍼 + 텍스처 SRV용 Descriptor Heap: 변수 의존은 없으나 렌더링 오브젝트에 종속. 공유하면 마지막에 쓴 값으로 렌더링
		// Create a descriptor heap for the constant buffers.
		// ★★★★★★★★★★★★★★★

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = numDescriptor;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		hr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbv.dscCbvHeap));
		if (FAILED(hr))
			return hr;
		cbv.descHandle = cbv.dscCbvHeap->GetGPUDescriptorHandleForHeapStart();

		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 6. Descriptor Heap 의존: 상수 버퍼용 리소스, 뷰: 변수 의존은 없으나 상수 버퍼 디스크립션 힙에 의존. 공유하면 마지막에 쓴 값으로 렌더링
		// Create a constant buffer descriptor heap and view for constant buffers.
		
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cbv.dscCbvHeap->GetCPUDescriptorHandleForHeapStart();
		for (size_t k=0; k<cbv.cbvRpl.size(); ++k)
		{
			CD3DX12_RESOURCE_DESC cbd = CD3DX12_RESOURCE_DESC::Buffer(FRAME_BUFFER_COUNT * cbv.cbvRpl[k].len);
			hr = d3dDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE
												, &cbd, D3D12_RESOURCE_STATE_GENERIC_READ
												, nullptr, IID_PPV_ARGS(&cbv.cbvRpl[k].rsc));
			if (FAILED(hr))
				return hr;
			CD3DX12_RANGE readRange(0, 0);
			hr = cbv.cbvRpl[k].rsc->Map(0, &readRange, reinterpret_cast<void**>(&cbv.cbvRpl[k].ptr));
			if (FAILED(hr))
				return hr;

			// 4. 상수 버퍼 뷰 생성.
			D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = cbv.cbvRpl[k].rsc->GetGPUVirtualAddress();
			UINT bufSize = cbv.cbvRpl[k].len;
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};

			for (UINT n = 0; n < FRAME_BUFFER_COUNT; ++n)
			{
				desc.BufferLocation = gpuAddress;
				desc.SizeInBytes = bufSize;
				d3dDevice->CreateConstantBufferView(&desc, cpuHandle);
				gpuAddress += desc.SizeInBytes;
				cpuHandle.ptr += descriptorSize;
			}
		}	

		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 7. Descriptor Heap 의존: 텍스처 디스크립터: 6.의 연장
		// Create Texture register SRV descriptor.
		// FRAME_BUFFER_COUNT * 상수 버퍼 레지스터 수만큼 다음이 텍스처 레지스터 시작.
		UINT baseSRVIndex = FRAME_BUFFER_COUNT * (UINT)cbv.cbvRpl.size();
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(cbv.dscCbvHeap->GetCPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(cbv.dscCbvHeap->GetGPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);

		// texture viewer 생성
		for(UINT kk=0; kk<(UINT)cbv.srvTex.size(); ++kk)
		{
			cbv.srvTex[ kk ] = hGpuSrv;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = m_textureLst[ kk ]->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			d3dDevice->CreateShaderResourceView(m_textureLst[ kk ], &srvDesc, hCpuSrv);
			hCpuSrv.ptr += descriptorSize;
			hGpuSrv.ptr += descriptorSize;
		}
	}
	return S_OK;
}


int MainApp::SetupResource()
{
	HRESULT hr = S_OK;
	auto d3dDevice      = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto commandAlloc   = std::any_cast<ID3D12CommandAllocator*>(IG2GraphicsD3D::getInstance()->GetCommandAllocator());
	auto commandList    = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// Data update
	// 리소스 버텍스 버퍼
	// 버텍스 버퍼는 CreateCommittedResource 내부에서 heap 사용?
	// 자원의 생성은 해당 hlsl 맞게 설정하면 되나, 지금은 정점 버퍼 생성과 동시에 gpu에 업로드 함.
	// commitList Reset-> command que  excute -> gpu wait 과정

	// ※ 정점 버퍼 업로드가 있음. commandList->ResourceBarrier ..
	// commandlist alloc 이 필요

	Vertex cubeVertices[] =
	{
		{{-120.0f,  120.0f, -120.0f}, {  0,   0, 255, 255}, {1.0f, 0.0f}},
		{{ 120.0f,  120.0f, -120.0f}, {  0, 255,   0, 255}, {0.0f, 0.0f}},
		{{ 120.0f,  120.0f,  120.0f}, {  0, 255, 255, 255}, {0.0f, 0.0f}},
		{{-120.0f,  120.0f,  120.0f}, {255,   0,   0, 255}, {1.0f, 0.0f}},
		{{-120.0f, -120.0f, -120.0f}, {255,   0, 255, 255}, {1.0f, 1.0f}},
		{{ 120.0f, -120.0f, -120.0f}, {255, 255,   0, 255}, {0.0f, 1.0f}},
		{{ 120.0f, -120.0f,  120.0f}, {255, 255, 255, 255}, {0.0f, 1.0f}},
		{{-120.0f, -120.0f,  120.0f}, { 70,  70,  70, 255}, {1.0f, 1.0f}},
	};
	unsigned short indices[] =
	{
		3, 1, 0, 2, 1, 3,
		0, 5, 4, 1, 5, 0,
		3, 4, 7, 0, 4, 3,
		1, 6, 5, 2, 6, 1,
		2, 7, 6, 3, 7, 2,
		6, 4, 5, 7, 4, 6,
	};

	UINT strideVtx = sizeof(Vertex);
	UINT strideIdx = sizeof(uint16_t);

	// setup vertex, index count
	m_vtxCount = sizeof(cubeVertices) / strideVtx;
	m_idxCount = sizeof(indices     ) / strideIdx;

	// 업로더는 ExecuteCommandLists 완료 될 때가지 유효해야함.
	ComPtr<ID3D12Resource>	cpuUploaderVtx{};	// vertex buffer upload heap resource
	ComPtr<ID3D12Resource>	cpuUploaderIdx{};	// index buffer upload heap resource
	{
		// vertex buffer
		CD3DX12_HEAP_PROPERTIES vtxHeapPropsGPU		(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_HEAP_PROPERTIES vtxHeapPropsUpload	(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC   vtxBufDesc			= CD3DX12_RESOURCE_DESC::Buffer(m_vtxCount * strideVtx);

		// GPU용 버텍스 버퍼
		hr = d3dDevice->CreateCommittedResource(&vtxHeapPropsGPU, D3D12_HEAP_FLAG_NONE, &vtxBufDesc
												, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_vtxGPU));
		if (FAILED(hr))
			return hr;

		// CPU 업로드 버퍼
		hr = d3dDevice->CreateCommittedResource(&vtxHeapPropsUpload, D3D12_HEAP_FLAG_NONE, &vtxBufDesc
												, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cpuUploaderVtx));
		if (FAILED(hr))
			return hr;

		// setup the buffer view.
		m_vtxView.BufferLocation = m_vtxGPU->GetGPUVirtualAddress();
		m_vtxView.StrideInBytes  = strideVtx;
		m_vtxView.SizeInBytes    = m_vtxCount * m_vtxView.StrideInBytes;
	}

	{
		// index buffer
		CD3DX12_HEAP_PROPERTIES idxHeapPropsGPU		(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_HEAP_PROPERTIES idxHeapPropsUpload	(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC	idxBufDesc			= CD3DX12_RESOURCE_DESC::Buffer(m_idxCount * strideIdx);

		// GPU용 인덱스 버퍼
		hr = d3dDevice->CreateCommittedResource(&idxHeapPropsGPU, D3D12_HEAP_FLAG_NONE, &idxBufDesc
												, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_idxGPU));
		if (FAILED(hr))
			return hr;

		// CPU 업로드 버퍼
		hr = d3dDevice->CreateCommittedResource(&idxHeapPropsUpload, D3D12_HEAP_FLAG_NONE, &idxBufDesc
												, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cpuUploaderIdx));
		if (FAILED(hr))
			return hr;

		// setup the buffer view.
		m_idxView.BufferLocation = m_idxGPU->GetGPUVirtualAddress();
		m_idxView.SizeInBytes    = m_idxCount * strideIdx;
		m_idxView.Format         = DXGI_FORMAT_R16_UINT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// beging upload to gpu memory

	hr = commandAlloc->Reset();
	if(FAILED(hr))
		return hr;
	hr = commandList->Reset(commandAlloc, nullptr);
	if(FAILED(hr))
		return hr;

	// setup the vertex uploading
	{
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData      = reinterpret_cast<const BYTE*>(cubeVertices);
		vertexData.RowPitch   = m_vtxCount * sizeof(Vertex);
		vertexData.SlicePitch = m_vtxCount * sizeof(Vertex);
		UpdateSubresources(commandList, m_vtxGPU.Get(), cpuUploaderVtx.Get(), 0, 0, 1, &vertexData);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_vtxGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		commandList->ResourceBarrier(1, &barrier);
	}

	// setup the index bufer uploading
	{
		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData      = reinterpret_cast<const BYTE*>(indices);
		indexData.RowPitch   = m_idxCount * sizeof(uint16_t);
		indexData.SlicePitch = m_idxCount * sizeof(uint16_t);
		UpdateSubresources(commandList, m_idxGPU.Get(), cpuUploaderIdx.Get(), 0, 0, 1, &indexData);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_idxGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		commandList->ResourceBarrier(1, &barrier);
	}

	hr = commandList->Close();
	if(FAILED(hr))
		return hr;

	// execute the uploading
	ID3D12CommandList* ppCommandLists[] = {commandList};
	auto commandQue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
	commandQue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// waiting for ExecuteCommandLists complete
	// uploader 변수가 유효하도록 gpu 완료를 기다림
	// ComPtr<ID3D12Resource>	cpuUploaderVtx{};
	// ComPtr<ID3D12Resource>	cpuUploaderIdx{};
	D3DApp::getInstance()-> WaitForGpu();

	return S_OK;
}

int MainApp::InitDefaultConstant()
{
	auto screenSize = std::any_cast<::SIZE*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_SCREEN_SIZE));
	float aspectRatio = 1280.0F / 640.0F;

	static const XMVECTORF32 eye = {0.0f, 0.0f, -2000.0f, 0.0f};
	static const XMVECTORF32 at  = {0.0f, 0.0f, 0.0f, 0.0f};
	static const XMVECTORF32 up  = {0.0f, 1.0f, 0.0f, 0.0f};

	for(size_t i=0; i< m_objValue.size(); ++i)
	{
		m_objValue[i].tmPrj = XMMatrixPerspectiveFovLH(XM_PIDIV4 * 1.2F, aspectRatio, 1.0f, 5000.0f);
		m_objValue[i].tmViw = XMMatrixLookAtLH(eye, at, up);
	}

	return S_OK;
}

