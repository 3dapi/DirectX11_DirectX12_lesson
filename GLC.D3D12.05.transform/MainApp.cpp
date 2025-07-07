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

const static int ALIGNED_MATRIX_SIZE = G2::align256BufferSize(sizeof(XMMATRIX));

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
	m_cbvHeap.Reset();
	m_rootSignature.Reset();
	m_pipelineState.Reset();
	m_viewVtx = {};
	m_viewIdx = {};
	m_numVtx = 0;
	m_numIdx = 0;
	m_cbvHandle.ptr = 0;

	m_cnstTmWld->Unmap(0, nullptr);
	m_cnstTmViw->Unmap(0, nullptr);
	m_cnstTmPrj->Unmap(0, nullptr);
	m_ptrWld = nullptr;
	m_ptrViw = nullptr;
	m_ptrPrj = nullptr;
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
		auto tmWld = XMMatrixRotationY((float)m_angle);
		m_tmWld[0] = tmWld;
		m_tmWld[1] = XMMatrixScaling(0.5F, 0.5F, 0.5F) * XMMatrixRotationY(FLOAT(m_angle)*2.0F) * XMMatrixTranslation(-400,    0, 0);
		m_tmWld[2] = XMMatrixScaling(0.7F, 0.7F, 0.7F) * XMMatrixRotationY(FLOAT(m_angle)*0.5F) * XMMatrixTranslation( 400,    0, 0);
		m_tmWld[3] = XMMatrixScaling(0.4F, 0.4F, 0.4F) * XMMatrixRotationY(FLOAT(m_angle)*3.0F) * XMMatrixTranslation(   0, -450, 0);
		m_tmWld[4] = XMMatrixScaling(0.3F, 0.3F, 0.3F) * XMMatrixRotationY(FLOAT(m_angle)*0.3F) * XMMatrixTranslation(   0,  350, 0);

		auto currentFrameIndex = *(std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));
		{
			uint8_t* dest = m_ptrViw + (currentFrameIndex * ALIGNED_MATRIX_SIZE);
			printf("m_ptrViw: Update Frame %u - -> 0x%p\n", currentFrameIndex, dest);
			memcpy(dest, &m_tmViw, sizeof(m_tmViw));
		}
		{
			uint8_t* dest = m_ptrPrj + (currentFrameIndex * ALIGNED_MATRIX_SIZE);
			printf("m_ptrPrj: Update Frame %u - -> 0x%p\n", currentFrameIndex, dest);
			memcpy(dest, &m_tmPrj, sizeof(m_tmPrj));
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
	UINT alignedSize       = ALIGNED_MATRIX_SIZE;

	// 1. 루트 시그너처
	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());

	// 2. 디스크립터 힙 설정
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	UINT b0_count = FRAME_BUFFER_COUNT * m_numObject;
	UINT b1_count = FRAME_BUFFER_COUNT * 1;
	UINT b2_count = FRAME_BUFFER_COUNT * 1;

	UINT b0_offset = 0;
	UINT b1_offset = b0_offset + b0_count;
	UINT b2_offset = b1_offset + b1_count;

	auto cbv_base = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
	auto cbvWldStart = CD3DX12_GPU_DESCRIPTOR_HANDLE(cbv_base, b0_offset, descriptorSize);
	auto cbvViwStart = CD3DX12_GPU_DESCRIPTOR_HANDLE(cbv_base, b1_offset, descriptorSize);
	auto cbvPrjStart = CD3DX12_GPU_DESCRIPTOR_HANDLE(cbv_base, b2_offset, descriptorSize);

	// draw
	auto cbv_begin = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
	for (int i = 0; i < 5; ++i)
	{
		UINT bufferIndex = currentFrameIndex * m_numObject + i;
		uint8_t* dest = m_ptrWld + bufferIndex * alignedSize;

		printf("m_ptrWld: Update Frame %u - -> 0x%p\n", currentFrameIndex, dest);

		memcpy(dest, &m_tmWld[i], sizeof(XMMATRIX));

		printf("Update Frame %u - Object %d   bufferIndex: %u \n", currentFrameIndex, i, bufferIndex);

		// 3. CBV 핸들 계산 및 바인딩 (root parameter index 0 = b0)
		CD3DX12_GPU_DESCRIPTOR_HANDLE handleWld(cbvWldStart, bufferIndex, descriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE handleViw(cbvViwStart, currentFrameIndex, descriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE handlePrj(cbvPrjStart, currentFrameIndex, descriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, handleWld); // b0 = world matrix
		cmdList->SetGraphicsRootDescriptorTable(1, handleViw);
		cmdList->SetGraphicsRootDescriptorTable(2, handlePrj);

		// 4. SRV 핸들 바인딩 (root parameter index 상수 레지스터 다음 3 = t0, 4= t1)
		cmdList->SetGraphicsRootDescriptorTable(3, m_srvHandleChecker);
		cmdList->SetGraphicsRootDescriptorTable(4, m_srvHandleXlogo);
		// 5. 파이프라인 연결
		cmdList->SetPipelineState(m_pipelineState.Get());
		// 토폴로지
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// 버택스, 인덱스 버퍼
		cmdList->IASetVertexBuffers(0, 1, &m_viewVtx);
		cmdList->IASetIndexBuffer(&m_viewIdx);
		cmdList->DrawIndexedInstanced(m_numIdx, 1, 0, 0, 0);
	}

	return S_OK;
}


int MainApp::InitResource()
{
	HRESULT hr = S_OK;
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto commandAlloc = std::any_cast<ID3D12CommandAllocator*>(IG2GraphicsD3D::getInstance()->GetCommandAllocator());
	auto commandList = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 1. Create a root signature with a single constant buffer slot.
	{
		// sampler register 갯수는 상관 없음.
		CD3DX12_STATIC_SAMPLER_DESC staticSampler[] =
		{
			{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 4, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			//{ 5, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			//{ 6, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			//{ 7, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			//{ 8, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			//{ 9, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
		};

		// 5 = 상수 레지스터 3 + 텍스처 레지스터 2
		CD3DX12_DESCRIPTOR_RANGE descRange[5];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0
		descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // b1
		descRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2); // b2
		descRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
		descRange[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

		CD3DX12_ROOT_PARAMETER rootParams[5];
		rootParams[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_VERTEX);		// cbv
		rootParams[1].InitAsDescriptorTable(1, &descRange[1], D3D12_SHADER_VISIBILITY_VERTEX);		// cbv
		rootParams[2].InitAsDescriptorTable(1, &descRange[2], D3D12_SHADER_VISIBILITY_VERTEX);		// cbv
		rootParams[3].InitAsDescriptorTable(1, &descRange[3], D3D12_SHADER_VISIBILITY_PIXEL);		// srv
		rootParams[4].InitAsDescriptorTable(1, &descRange[4], D3D12_SHADER_VISIBILITY_PIXEL);		// src

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init(
			_countof(rootParams),
			rootParams,
			5,					// sampler register 숫자.
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
		hr = G2::DXCompileShaderFromFile("Shaders/simple.hlsl", "vs_5_0", "main_vs", &shaderVtx);
		if(FAILED(hr))
			return hr;
		hr = G2::DXCompileShaderFromFile("Shaders/simple.hlsl", "ps_5_0", "main_ps", &shaderPxl);
		if(FAILED(hr))
			return hr;
	}
	// Create the pipeline state once the shaders are loaded.
	{
		const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM , 0, sizeof(XMFLOAT3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT   , 0, sizeof(XMFLOAT3) + sizeof(uint32_t), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
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
	// 3. 상수 + 텍스처 버퍼 heap 생성
	// Create a descriptor heap for the constant buffers.
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors =
							  FRAME_BUFFER_COUNT * m_numObject				// per-object b0
							+ FRAME_BUFFER_COUNT * (m_numRegisterConst - 1) // shared b1, b2
							+ m_numRegisterTex;								// SRV for textures

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
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		UINT width = FRAME_BUFFER_COUNT * m_numObject * ALIGNED_MATRIX_SIZE;
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(width);
		hr = d3dDevice->CreateCommittedResource(&heapProperties
												, D3D12_HEAP_FLAG_NONE
												, &constantBufferDesc
												, D3D12_RESOURCE_STATE_GENERIC_READ
												, nullptr, IID_PPV_ARGS(&m_cnstTmWld));
		if(FAILED(hr))
			return hr;
		CD3DX12_RANGE readRange(0, 0);
		hr = m_cnstTmWld->Map(0, &readRange, reinterpret_cast<void**>(&m_ptrWld));
		if (FAILED(hr))
			return hr;
	}
	{
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		UINT width = FRAME_BUFFER_COUNT * 1 * ALIGNED_MATRIX_SIZE;
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(width);
		hr = d3dDevice->CreateCommittedResource(&heapProperties
												, D3D12_HEAP_FLAG_NONE
												, &constantBufferDesc
												, D3D12_RESOURCE_STATE_GENERIC_READ
												, nullptr, IID_PPV_ARGS(&m_cnstTmViw));
		if(FAILED(hr))
			return hr;
		CD3DX12_RANGE readRange(0, 0);
		hr = m_cnstTmViw->Map(0, &readRange, reinterpret_cast<void**>(&m_ptrViw));
		if (FAILED(hr))
			return hr;
	}
	{
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		UINT width = FRAME_BUFFER_COUNT * 1 * ALIGNED_MATRIX_SIZE;
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(width);
		hr = d3dDevice->CreateCommittedResource(&heapProperties
												, D3D12_HEAP_FLAG_NONE
												, &constantBufferDesc
												, D3D12_RESOURCE_STATE_GENERIC_READ
												, nullptr, IID_PPV_ARGS(&m_cnstTmPrj));
		if(FAILED(hr))
			return hr;
		CD3DX12_RANGE readRange(0, 0);
		hr = m_cnstTmPrj->Map(0, &readRange, reinterpret_cast<void**>(&m_ptrPrj));
		if (FAILED(hr))
			return hr;
	}

	UINT d3dDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	UINT b0_count = FRAME_BUFFER_COUNT * m_numObject;
	UINT b1_count = FRAME_BUFFER_COUNT;
	UINT b2_count = FRAME_BUFFER_COUNT;

	UINT b0_offset = 0;
	UINT b1_offset = b0_offset + b0_count;
	UINT b2_offset = b1_offset + b1_count;

	const UINT alignedSize = ALIGNED_MATRIX_SIZE;

	// -----------------------------------------------------------------------------
	// b0: World Matrix (per-object, per-frame)
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
	size_t debug_begin_0 = cpuHandle.ptr;
	size_t debug_begin_1 = cpuHandle.ptr;
	size_t debug_begin_2 = cpuHandle.ptr;
	{
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = m_cnstTmWld->GetGPUVirtualAddress();
		for (UINT n = 0; n < b0_count; ++n)
		{
			printf("m_cnstTmWld: CBV[%d] GPUAddress = 0x%llx\n", n, gpuAddress);
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
			desc.BufferLocation = gpuAddress;
			desc.SizeInBytes = alignedSize;
			d3dDevice->CreateConstantBufferView(&desc, cpuHandle);
			gpuAddress += desc.SizeInBytes;
			cpuHandle.ptr += d3dDescriptorSize;
		}
	}
	// -----------------------------------------------------------------------------
	// b1: View Matrix (per-frame)
	{
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = m_cnstTmViw->GetGPUVirtualAddress();
	//	cpuHandle.ptr += b1_offset * d3dDescriptorSize;
		debug_begin_1 = debug_begin_1 + b1_offset * d3dDescriptorSize;

		for (UINT n = 0; n < b1_count; ++n)
		{
			printf("m_cnstTmViw: CBV[%d] GPUAddress = 0x%llx\n", n, gpuAddress);
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
			desc.BufferLocation = gpuAddress;
			desc.SizeInBytes = alignedSize;
			d3dDevice->CreateConstantBufferView(&desc, cpuHandle);
			gpuAddress += desc.SizeInBytes;
			cpuHandle.ptr += d3dDescriptorSize;
		}
	}
	// -----------------------------------------------------------------------------
	// b2: Projection Matrix (per-frame)
	{
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = m_cnstTmPrj->GetGPUVirtualAddress();
		//cpuHandle.ptr += b2_offset * d3dDescriptorSize;
		debug_begin_2 = debug_begin_2 + b2_offset * d3dDescriptorSize;

		for (UINT n = 0; n < b2_count; ++n)
		{
			printf("m_cnstTmPrj: CBV[%d] GPUAddress = 0x%llx\n", n, gpuAddress);
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
			desc.BufferLocation = gpuAddress;
			desc.SizeInBytes = alignedSize;
			d3dDevice->CreateConstantBufferView(&desc, cpuHandle);
			gpuAddress += desc.SizeInBytes;
			cpuHandle.ptr += d3dDescriptorSize;
		}
	}


	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 4. 텍스처 생성 및 업로드 (CreateWICTextureFromFile + ResourceUploadBatch)
	{
		DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
		{
			resourceUpload.Begin();
			hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, L"assets/res_checker.png", m_textureChecker.GetAddressOf());
			if(FAILED(hr))
				return hr;
			auto commandQueue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
			auto uploadOp = resourceUpload.End(commandQueue);
			uploadOp.wait();  // GPU 업로드 완료 대기
		}
	}
	{
		DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
		{
			resourceUpload.Begin();
			hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, L"assets/xlogo.png", m_textureXlogo.GetAddressOf());
			if(FAILED(hr))
				return hr;
			auto commandQueue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
			auto uploadOp = resourceUpload.End(commandQueue);
			uploadOp.wait();  // GPU 업로드 완료 대기
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 5 텍스처 레지스터 SRV 디스크립터 생성 
	// 3개 상수 레지스터 다음부터 텍스처 레지스터(셰이더 참고)
	//FRAME_BUFFER_COUNT * 3
	UINT baseSRVIndex = FRAME_BUFFER_COUNT * m_numRegisterConst;
	UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);

	// texture viewer 생성
	{
		hCpuSrv.Offset(0, descriptorSize);
		hGpuSrv.Offset(0, descriptorSize);
		m_srvHandleChecker = hGpuSrv;					// CPU, GPU OFFSET을 이동후 Heap pointer 위치를 저장 이 핸들 값이 텍스처 핸들

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = m_textureChecker->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		d3dDevice->CreateShaderResourceView(m_textureChecker.Get(), &srvDesc, hCpuSrv);
	}
	// t1 : xlogo
	{
		hCpuSrv.Offset(1, descriptorSize);
		hGpuSrv.Offset(1, descriptorSize);
		m_srvHandleXlogo = hGpuSrv;						// CPU, GPU OFFSET을 이동후 Heap pointer 위치를 저장 이 핸들 값이 텍스처 핸들
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = m_textureXlogo->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		d3dDevice->CreateShaderResourceView(m_textureXlogo.Get(), &srvDesc, hCpuSrv);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 리소스 버텍스 버퍼
	// 버텍스 버퍼는 CreateCommittedResource 내부에서 heap 사용?
	// 자원의 생성은 해당 hlsl 맞게 설정하면 되나, 지금은 정점 버퍼 생성과 동시에 gpu에 업로드 함.
	// commitList Reset-> command que  excute -> gpu wait 과정이 있어 맨 뒤로 옮김
	{
		// ※ 정점 버퍼 업로드가 있음. commandList->ResourceBarrier ..
		// commandlist alloc 이 필요
		hr = commandAlloc->Reset();
		if(FAILED(hr))
			return hr;
		hr = commandList->Reset(commandAlloc, nullptr);  // PSO는 루프 내에서 설정 예정
		if(FAILED(hr))
			return hr;

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
			3, 1, 0, 2, 1, 3,
			0, 5, 4, 1, 5, 0,
			3, 4, 7, 0, 4, 3,
			1, 6, 5, 2, 6, 1,
			2, 7, 6, 3, 7, 2,
			6, 4, 5, 7, 4, 6,
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
	auto screenSize = std::any_cast<::SIZE*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_SCREEN_SIZE));
	float aspectRatio = 1280.0F / 640.0F;

	m_tmPrj = XMMatrixPerspectiveFovLH(XM_PIDIV4 * 1.2F, aspectRatio, 1.0f, 5000.0f);

	static const XMVECTORF32 eye = {0.0f, 0.0f, -2000.0f, 0.0f};
	static const XMVECTORF32 at  = {0.0f, 0.0f, 0.0f, 0.0f};
	static const XMVECTORF32 up  = {0.0f, 1.0f, 0.0f, 0.0f};
	m_tmViw = XMMatrixLookAtLH(eye, at, up);
	return S_OK;
}