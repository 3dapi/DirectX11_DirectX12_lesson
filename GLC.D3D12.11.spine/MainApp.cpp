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

namespace spine {
	spine::SpineExtension* getDefaultExtension()
	{
		static spine::SpineExtension* _default_spineExtension = new spine::DefaultSpineExtension;
		return _default_spineExtension;
	}
}

MainApp::MainApp()
{
}

MainApp::~MainApp()
{
	Destroy();
}

void MainApp::load(spine::AtlasPage& page, const spine::String& path) {
}
void MainApp::unload(void* texture) {
}

int MainApp::Init()
{
	m_timer.Reset();

	HRESULT hr = S_OK;
	hr = InitResource();
	if(FAILED(hr))
		return hr;
	hr = InitConstValue();
	if(FAILED(hr))
		return hr;

	return S_OK;
}

int MainApp::Destroy()
{
	m_cbvHeap		.Reset();
	m_rootSignature	.Reset();
	m_pipelineState	.Reset();

	m_viewVtx		= {};
	m_viewIdx		= {};
	m_numVtx		= {};
	m_numIdx		= {};
	m_cbvHandle		= {};

	m_rscVtx		.Reset();
	m_rscIdx		.Reset();

	m_cnstTmWld		->Unmap(0, nullptr);
	m_cnstTmWld		.Reset();
	m_ptrWld		= {};
	m_textureRsc	.Reset();
	m_textureHandle	= {};

	return S_OK;
}

int MainApp::Update()
{
	m_timer.Tick();
	auto deltaTime = m_timer.DeltaTime();

	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());

	if(true)
	{
		m_angle += deltaTime ;
		m_angle = 0.0F;
		float aspectRatio = 1280.0F / 640.0F;
		XMMATRIX tmPrj = XMMatrixPerspectiveFovLH(XM_PIDIV4 * 1.2F, aspectRatio, 1.0f, 5000.0f);
		static const XMVECTORF32 eye = {0.0f, 10.0f, -900.0f, 0.0f};
		static const XMVECTORF32 at = {0.0f, 0.0f, 0.0f, 0.0f};
		static const XMVECTORF32 up = {0.0f, 1.0f, 0.0f, 0.0f};
		XMMATRIX tmViw = XMMatrixLookAtLH(eye, at, up);
		XMMATRIX tmWld = XMMatrixRotationY((float)m_angle);
		XMMATRIX mtMVP = tmWld * tmViw * tmPrj;

		auto currentFrameIndex = *(std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));
		{
			uint8_t* dest = m_ptrWld + (currentFrameIndex * G2::align256BufferSize(sizeof(XMMATRIX)));
			memcpy(dest, &mtMVP, sizeof(mtMVP));
		}
	}
	return S_OK;
}

int MainApp::Render()
{
	HRESULT hr = S_OK;
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto cmdList = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	UINT currentFrameIndex = *(std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));
	UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 1. 루트 시그너처
	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());

	// 2. 디스크립터 힙 설정
	ID3D12DescriptorHeap* descriptorHeaps[] = {m_cbvHeap.Get()};
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// 3. CBV 핸들 계산 및 바인딩 (root parameter index 0 = b0)
	CD3DX12_GPU_DESCRIPTOR_HANDLE handleMVP(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), 0 * FRAME_BUFFER_COUNT + currentFrameIndex, descriptorSize);

	cmdList->SetGraphicsRootDescriptorTable(0, handleMVP);

	// 4. SRV 핸들 바인딩 (root parameter index 상수 레지스터 다음 3 = t0, 4= t1)
	cmdList->SetGraphicsRootDescriptorTable(1, m_textureHandle);

	// 5. 파이프라인 연결
	cmdList->SetPipelineState(m_pipelineState.Get());


	// 토폴로지
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 버택스, 인덱스 버퍼
	cmdList->IASetVertexBuffers(0, 1, &m_viewVtx);
	cmdList->IASetIndexBuffer(&m_viewIdx);

	// draw
	cmdList->DrawIndexedInstanced(m_numIdx, 1, 0, 0, 0);

	return S_OK;
}


int MainApp::InitResource()
{
	HRESULT hr = S_OK;
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto commandAlloc = std::any_cast<ID3D12CommandAllocator*>(IG2GraphicsD3D::getInstance()->GetCommandAllocator());
	auto commandList = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());

	hr = commandAlloc->Reset();
	if(FAILED(hr))
		return hr;
	hr = commandList->Reset(commandAlloc, nullptr);  // PSO는 루프 내에서 설정 예정
	if(FAILED(hr))
		return hr;

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// ★★★★★★★★★★★★★★★
	const UINT NUM_CB = 1;						// 셰이더 상수 레지스터 숫자
	const UINT NUM_TX = 1;						// 셰이더 텍스처 텍스처 레지스터
	const UINT NUM_CB_TX = NUM_CB + NUM_TX;
	//1. Create a root signature with a single constant buffer slot.
	{
		// sampler register 갯수는 상관 없음.
		CD3DX12_STATIC_SAMPLER_DESC staticSampler[] =
		{
			{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
		};

		// 2 = 상수 레지스터 1 + 텍스처 레지스터 1
		CD3DX12_DESCRIPTOR_RANGE descRange[NUM_CB_TX];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0
		descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

		CD3DX12_ROOT_PARAMETER rootParams[2];
		rootParams[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_VERTEX);		// cbv
		rootParams[1].InitAsDescriptorTable(1, &descRange[1], D3D12_SHADER_VISIBILITY_PIXEL);		// src

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init(
			_countof(rootParams),
			rootParams,
			2,					// sampler register 숫자.
			staticSampler,		// sampler register desc
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		);

		ComPtr<ID3DBlob> pSignature{}, pError{};
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if(FAILED(hr))
			return hr;
		hr = d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
		if(FAILED(hr))
			return hr;

	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 2. pipe line 설정
	// compile shader
	ComPtr<ID3DBlob> shaderVtx{}, shaderPxl{};
	{
		HRESULT hr = S_OK;
		hr = G2::DXCompileShaderFromFile("Shaders/spine.hlsl", "vs_5_0", "main_vs", &shaderVtx);
		if(FAILED(hr))
			return hr;
		hr = G2::DXCompileShaderFromFile("Shaders/spine.hlsl", "ps_5_0", "main_ps", &shaderPxl);
		if(FAILED(hr))
			return hr;
	}
	// Create the pipeline state once the shaders are loaded.
	{
		const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT    , 0, 0                                  , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM  , 0, sizeof(XMFLOAT2)                   , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT    , 0, sizeof(XMFLOAT2) + sizeof(uint32_t), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
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
		if(FAILED(hr))
			return hr;
	};

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 3. 리소스 버텍스 버퍼
	// 버텍스 버퍼는 CreateCommittedResource 내부에서 heap 사용?
	{
		Vertex cubeVertices[] =
		{
			{{-200.0f,  200.0f}, {255,   0,   0, 255}, {0.0f, 0.0f}},
			{{ 200.0f,  200.0f}, {  0, 255,   0, 255}, {1.0f, 0.0f}},
			{{ 200.0f, -200.0f}, {  0,   0, 255, 255}, {1.0f, 1.0f}},
			{{-200.0f, -200.0f}, {255,   0, 255, 255}, {0.0f, 1.0f}},
		};
		const UINT vertexBufferSize = sizeof(cubeVertices);
		ComPtr<ID3D12Resource> vertexBufferUpload;
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		hr = d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_rscVtx));
		if(FAILED(hr))
			return hr;

		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		hr = d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBufferUpload));
		if(FAILED(hr))
			return hr;

		// Upload the vertex buffer to the GPU.
		{
			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData = reinterpret_cast<BYTE*>(cubeVertices);
			vertexData.RowPitch = vertexBufferSize;
			vertexData.SlicePitch = vertexData.RowPitch;

			UpdateSubresources(commandList, m_rscVtx.Get(), vertexBufferUpload.Get(), 0, 0, 1, &vertexData);

			CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_rscVtx.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			commandList->ResourceBarrier(1, &vertexBufferResourceBarrier);
		}

		unsigned short indices[] =
		{
			0, 1, 2, 0, 2, 3,
		};

		m_numIdx = sizeof(indices) / sizeof(indices[0]);
		const UINT indexBufferSize = m_numIdx * sizeof(unsigned short);

		ComPtr<ID3D12Resource> indexBufferUpload{};

		CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		hr = d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_rscIdx));
		if(FAILED(hr))
			return hr;
		hr = d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBufferUpload));
		if(FAILED(hr))
			return hr;

		// Upload the index buffer to the GPU.
		{
			D3D12_SUBRESOURCE_DATA indexData = {};
			indexData.pData = reinterpret_cast<BYTE*>(indices);
			indexData.RowPitch = indexBufferSize;
			indexData.SlicePitch = indexData.RowPitch;

			UpdateSubresources(commandList, m_rscIdx.Get(), indexBufferUpload.Get(), 0, 0, 1, &indexData);

			CD3DX12_RESOURCE_BARRIER indexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_rscIdx.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
			commandList->ResourceBarrier(1, &indexBufferResourceBarrier);
		}

		// Create vertex/index buffer views.
		m_viewVtx.BufferLocation = m_rscVtx->GetGPUVirtualAddress();
		m_viewVtx.StrideInBytes = sizeof(Vertex);
		m_viewVtx.SizeInBytes = sizeof(cubeVertices);

		m_viewIdx.BufferLocation = m_rscIdx->GetGPUVirtualAddress();
		m_viewIdx.SizeInBytes = sizeof(indices);
		m_viewIdx.Format = DXGI_FORMAT_R16_UINT;

		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 4. 상수 + 텍스처 버퍼 heap 생성
		// Create a descriptor heap for the constant buffers.
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = FRAME_BUFFER_COUNT * 3  + 2;	// 프레임당 상수 버퍼 3개 (b0~b2) * 프레임 수 + 텍스처용 SRV(t0,t0) 2개
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			hr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvHeap));
			if(FAILED(hr))
				return hr;
			m_cbvHandle = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
		}
		
		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 상수 버퍼용 리소스 생성
		{
			CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(FRAME_BUFFER_COUNT * G2::align256BufferSize(sizeof XMMATRIX ));
			hr = d3dDevice->CreateCommittedResource(&uploadHeapProperties
													, D3D12_HEAP_FLAG_NONE
													, &constantBufferDesc
													, D3D12_RESOURCE_STATE_GENERIC_READ
													, nullptr, IID_PPV_ARGS(&m_cnstTmWld));
			if(FAILED(hr))
				return hr;
		}

		UINT d3dDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		const UINT bufferSize = G2::align256BufferSize(sizeof XMMATRIX);
		// b0: Wld
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_cnstTmWld->GetGPUVirtualAddress();
			D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
			// offset 0
			for(int n = 0; n < FRAME_BUFFER_COUNT; n++)
			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
				desc.BufferLocation = cbvGpuAddress;
				desc.SizeInBytes = bufferSize;
				d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);
				cbvGpuAddress += bufferSize;
				cbvCpuHandle.ptr += d3dDescriptorSize;
			}
			CD3DX12_RANGE readRange(0, 0);
			hr = m_cnstTmWld->Map(0, &readRange, reinterpret_cast<void**>(&m_ptrWld));
			if(FAILED(hr)) return hr;
		}

		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 5 텍스처 생성 및 업로드 (CreateWICTextureFromFile + ResourceUploadBatch)
		{
			DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
			{
				resourceUpload.Begin();
				hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, L"assets/res_checker.png", m_textureRsc.GetAddressOf());
				if(FAILED(hr))
					return hr;
				auto commandQueue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
				auto uploadOp = resourceUpload.End(commandQueue);
				uploadOp.wait();  // GPU 업로드 완료 대기
			}
		}

		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 6 텍스처 레지스터 SRV 디스크립터 생성
		// 실수 많이함. rootSigDesc 초기화 부분과 일치해야함.
		// ★★★★★★★★★★★★★★★
		// 3개 상수 레지스터 다음부터 텍스처 레지스터(셰이더 참고)
		//FRAME_BUFFER_COUNT * NUM_CB
		UINT baseSRVIndex = FRAME_BUFFER_COUNT * NUM_CB;
		UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);

		// texture viewer 생성
		{
			hCpuSrv.Offset(0, descriptorSize);			//
			hGpuSrv.Offset(0, descriptorSize);			//
			m_textureHandle = hGpuSrv;					// CPU, GPU OFFSET을 이동후 Heap pointer 위치를 저장 이 핸들 값이 텍스처 핸들

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = m_textureRsc->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			d3dDevice->CreateShaderResourceView(m_textureRsc.Get(), &srvDesc, hCpuSrv);
		}

		// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		// 마지막으로 commandList를 닫고
		hr = commandList->Close();
		if(FAILED(hr))
			return hr;
		// 실행하고
		ID3D12CommandList* ppCommandLists[] = {commandList};
		auto commandQue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
		commandQue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		// gpu 완료 될 때 까지 기다림
		D3DApp::getInstance()-> WaitForGpu();
	}
	return S_OK;
}

int MainApp::InitConstValue()
{
	return S_OK;
}