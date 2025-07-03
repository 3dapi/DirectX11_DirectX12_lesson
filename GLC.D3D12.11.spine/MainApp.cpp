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
	{
		DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
		{
			resourceUpload.Begin();
			hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, L"assets/res_checker.png", m_spineTextureRsc.GetAddressOf());
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
	{
		UINT baseSRVIndex = FRAME_BUFFER_COUNT * NUM_CB;
		UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);

		// texture viewer 생성
		{
			hCpuSrv.Offset(0, descriptorSize);			//
			hGpuSrv.Offset(0, descriptorSize);			//
			m_spineTexture = hGpuSrv;					// CPU, GPU OFFSET을 이동후 Heap pointer 위치를 저장 이 핸들 값이 텍스처 핸들

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = m_spineTextureRsc->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			d3dDevice->CreateShaderResourceView(m_spineTextureRsc.Get(), &srvDesc, hCpuSrv);
		}
	}
}
void MainApp::unload(void* texture) {
}

int MainApp::Init()
{
	m_timer.Reset();

	HRESULT hr = S_OK;
	hr = InitForDevice();
	if(FAILED(hr))
		return hr;
	hr = InitForResource();
	if(FAILED(hr))
		return hr;
	hr = InitSpine();
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

	m_rscVtxGPU		.Reset();
	m_rscIdxGPU		.Reset();

	m_cnstMVP		->Unmap(0, nullptr);
	m_cnstMVP		.Reset();
	m_ptrMVP		= {};
	m_spineTextureRsc	.Reset();
	m_spineTexture	= {};

	return S_OK;
}

int MainApp::Update()
{
	m_timer.Tick();
	auto deltaTime = m_timer.DeltaTime();

	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());

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
		uint8_t* dest = m_ptrMVP + (currentFrameIndex * G2::align256BufferSize(sizeof(XMMATRIX)));
		memcpy(dest, &mtMVP, sizeof(mtMVP));
	}

	// Update and apply the animation state to the skeleton
	m_spineAniState->update(deltaTime);
	m_spineAniState->apply(*m_spineSkeleton);

	// Update the skeleton time (used for physics)
	m_spineSkeleton->update(deltaTime);

	// Calculate the new pose
	m_spineSkeleton->updateWorldTransform(spine::Physics_Update);

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
	cmdList->SetGraphicsRootDescriptorTable(1, m_spineTexture);

	// 5. 파이프라인 연결
	cmdList->SetPipelineState(m_pipelineState.Get());


	// 7. 정점 복사, draw
	auto drawOrder = m_spineSkeleton->getDrawOrder();
	for(size_t i = 0; i < drawOrder.size(); ++i)
	{
		spine::Slot* slot = drawOrder[i];
		spine::Attachment* attachment = slot->getAttachment();
		if(!attachment)
			continue;

		if(attachment->getRTTI().isExactly(spine::MeshAttachment::rtti))
		{
			auto* attm = static_cast<spine::MeshAttachment*>(attachment);
			auto* mesh = static_cast<spine::MeshAttachment*>(attachment);

			// 색상
			spine::Color c = slot->getColor();
			c.a *= slot->getColor().a;
			c.r *= slot->getColor().r;
			c.g *= slot->getColor().g;
			c.b *= slot->getColor().b;

			spine::Color mColor = mesh->getColor();
			c.r *= mColor.r * 255;
			c.g *= mColor.g * 255;
			c.b *= mColor.b * 255;
			c.a *= mColor.a * 255;
			uint32_t rgba = (uint32_t(c.a) << 24) | (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | (uint32_t(c.b) << 0);

			// 텍스처
			spine::TextureRegion* texRegion = mesh->getRegion();
			if(!texRegion) continue;

			auto* atlasRegion = reinterpret_cast<spine::AtlasRegion*>(texRegion);
			auto* texSRV = reinterpret_cast<ID3D11ShaderResourceView*>(atlasRegion->page->texture);
			if(!texSRV) continue;

			// 위치 변환 및 복사
			{
				size_t bufSize = mesh->getWorldVerticesLength();
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if(SUCCEEDED(d3dContext->Map(m_bufVtxPos, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
					float* ptr = (float*)mapped.pData;
					mesh->computeWorldVertices(*slot, 0, bufSize, ptr, 0, 2);
					d3dContext->Unmap(m_bufVtxPos, 0);
				}
			}
			// color
			{
				size_t vtxCount = mesh->getWorldVerticesLength() / 2;
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if(SUCCEEDED(d3dContext->Map(m_bufVtxDif, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
					uint32_t* ptr = (uint32_t*)mapped.pData;
					for(size_t i = 0; i < vtxCount; ++i)
					{
						ptr[i] = rgba;
					}
					d3dContext->Unmap(m_bufVtxDif, 0);
				}
			}
			// 텍스처 좌표 복사
			{
				const float* uvs = mesh->getUVs().buffer();
				auto bufSize = mesh->getUVs().size();
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if(SUCCEEDED(d3dContext->Map(m_bufVtxTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
					memcpy(mapped.pData, uvs, sizeof(float) * bufSize);
					d3dContext->Unmap(m_bufVtxTex, 0);
				}
			}
			// index buffer
			const uint16_t* indices = mesh->getTriangles().buffer();
			UINT indexCount = (UINT)mesh->getTriangles().size();
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if(SUCCEEDED(d3dContext->Map(m_bufIdx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
					memcpy(mapped.pData, indices, sizeof(uint16_t) * indexCount);
					d3dContext->Unmap(m_bufIdx, 0);
				}
			}

			// 바인딩
			ID3D11Buffer* buffers[] = {m_bufVtxPos, m_bufVtxDif, m_bufVtxTex};
			UINT strides[] = {sizeof(XMFLOAT2), sizeof(uint32_t), sizeof(XMFLOAT2)};
			UINT offsets[] = {0, 0, 0};
			d3dContext->IASetVertexBuffers(0, 3, buffers, strides, offsets);

			d3dContext->IASetIndexBuffer(m_bufIdx, DXGI_FORMAT_R16_UINT, 0);
			d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3dContext->PSSetShaderResources(0, 1, &texSRV);

			d3dContext->DrawIndexed(indexCount, 0, 0);
		}
		else if(attachment->getRTTI().isExactly(spine::RegionAttachment::rtti))
		{
			auto* region = static_cast<spine::RegionAttachment*>(attachment);
			// RegionAttachment → TextureRegion
			spine::TextureRegion* texRegion = region->getRegion();
			if(!texRegion)
				continue;
			// TextureRegion → AtlasRegion
			auto* atlasRegion = reinterpret_cast<spine::AtlasRegion*>(texRegion);
			// AtlasPage → rendererObject
			auto* page = atlasRegion->page;
			auto* texSRV = reinterpret_cast<ID3D11ShaderResourceView*>(page->texture);
			if(!texSRV)
				continue;

			// 색상 계산
			spine::Color c = m_spineSkeleton->getColor();
			c.a *= slot->getColor().a;
			c.r *= slot->getColor().r;
			c.g *= slot->getColor().g;
			c.b *= slot->getColor().b;

			spine::Color rColor = region->getColor();
			c.a *= rColor.a * 255;
			c.r *= rColor.r * 255;
			c.g *= rColor.g * 255;
			c.b *= rColor.b * 255;

			uint32_t rgba = (uint32_t(c.a) << 24) | (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | (uint32_t(c.b) << 0);

			// 위치 변환 및 복사
			{
				size_t vtxCount = 8;
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if(SUCCEEDED(d3dContext->Map(m_bufVtxPos, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
					float* ptr = (float*)mapped.pData;
					region->computeWorldVertices(*slot, ptr, 0, 2);
					d3dContext->Unmap(m_bufVtxPos, 0);
				}
			}
			// 텍스처 좌표 복사
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				const float* uvs = region->getUVs().buffer();
				auto uvSize = region->getUVs().size();
				if(SUCCEEDED(d3dContext->Map(m_bufVtxTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
				{
					float* ptr = (float*)mapped.pData;
					memcpy(mapped.pData, uvs, sizeof(float) * uvSize);
					d3dContext->Unmap(m_bufVtxTex, 0);
				}
			}
			// color 버퍼에 같은 색상 복사
			{
				size_t vtxCount = 4;
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if(SUCCEEDED(d3dContext->Map(m_bufVtxDif, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
				{
					uint32_t* ptr = (uint32_t*)mapped.pData;
					for(size_t i = 0; i < vtxCount; ++i)
					{
						ptr[i] = rgba;
					}
					d3dContext->Unmap(m_bufVtxDif, 0);
				}
			}

			// 정점 버퍼 바인딩
			ID3D11Buffer* buffers[] = {m_bufVtxPos, m_bufVtxDif, m_bufVtxTex};
			UINT strides[] = {sizeof(XMFLOAT2), sizeof(uint32_t), sizeof(XMFLOAT2)};
			UINT offsets[] = {0, 0, 0};
			d3dContext->IASetVertexBuffers(0, 3, buffers, strides, offsets);

			d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			d3dContext->PSSetShaderResources(0, 1, &texSRV);

			// draw
			d3dContext->Draw(4, 0);
		}
	}

	return S_OK;
}

int MainApp::InitForDevice()
{
	HRESULT hr = S_OK;
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// ★★★★★★★★★★★★★★★
	const UINT NUM_CB = 1;						// 셰이더 상수 레지스터 숫자
	const UINT NUM_TX = 1;						// 셰이더 텍스처 텍스처 레지스터
	const UINT NUM_CB_TX = NUM_CB + NUM_TX;
	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 1. 상수 + 텍스처 버퍼 heap 생성
	// Create a descriptor heap for the constant buffers.
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = FRAME_BUFFER_COUNT * (NUM_CB  + NUM_TX);	// 프레임당 상수 버퍼 3개 (b0~b2) * 프레임 수 + 텍스처용 SRV(t0,t0) 2개 대신 넉넉하게 설정
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		hr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvHeap));
		if(FAILED(hr))
			return hr;
		m_cbvHandle = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
	}

	//2. Create a root signature with a single constant buffer slot.
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
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 3. 상수 버퍼용 리소스 생성
	{
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(FRAME_BUFFER_COUNT * G2::align256BufferSize(sizeof XMMATRIX ));
		hr = d3dDevice->CreateCommittedResource(&uploadHeapProperties
												, D3D12_HEAP_FLAG_NONE
												, &constantBufferDesc
												, D3D12_RESOURCE_STATE_GENERIC_READ
												, nullptr, IID_PPV_ARGS(&m_cnstMVP));
		if(FAILED(hr))
			return hr;
	}

	{
		UINT d3dDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		const UINT bufferSize = G2::align256BufferSize(sizeof XMMATRIX);
		// b0: Wld
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_cnstMVP->GetGPUVirtualAddress();
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
			hr = m_cnstMVP->Map(0, &readRange, reinterpret_cast<void**>(&m_ptrMVP));
			if(FAILED(hr)) return hr;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 5 텍스처 생성 및 업로드 (CreateWICTextureFromFile + ResourceUploadBatch)
	{
		DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
		{
			resourceUpload.Begin();
			hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, L"assets/res_checker.png", m_spineTextureRsc.GetAddressOf());
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
	{
		UINT baseSRVIndex = FRAME_BUFFER_COUNT * NUM_CB;
		UINT descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), baseSRVIndex, descriptorSize);

		// texture viewer 생성
		{
			hCpuSrv.Offset(0, descriptorSize);			//
			hGpuSrv.Offset(0, descriptorSize);			//
			m_spineTexture = hGpuSrv;					// CPU, GPU OFFSET을 이동후 Heap pointer 위치를 저장 이 핸들 값이 텍스처 핸들

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = m_spineTextureRsc->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			d3dDevice->CreateShaderResourceView(m_spineTextureRsc.Get(), &srvDesc, hCpuSrv);
		}
	}

	return S_OK;
}

int MainApp::InitForResource()
{
	auto d3d          = IG2GraphicsD3D::getInstance();
	auto d3dDevice    = std::any_cast<ID3D12Device*             >(d3d->GetDevice()          );
	auto commandAlloc = std::any_cast<ID3D12CommandAllocator*   >(d3d->GetCommandAllocator());
	auto commandList  = std::any_cast<ID3D12GraphicsCommandList*>(d3d->GetCommandList()     );

	HRESULT hr = S_OK;

	hr = commandAlloc->Reset();
	if(FAILED(hr))
		return hr;
	
	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 7. 리소스 버텍스 버퍼 업로드
	// 버텍스 버퍼는 CreateCommittedResource 내부에서 heap 사용?
	Vertex cubeVertices[] =
	{
		{{-200.0f,  200.0f}, {255,   0,   0, 255}, {0.0f, 0.0f}},
		{{ 200.0f,  200.0f}, {  0, 255,   0, 255}, {1.0f, 0.0f}},
		{{ 200.0f, -200.0f}, {  0,   0, 255, 255}, {1.0f, 1.0f}},
		{{-200.0f, -200.0f}, {255,   0, 255, 255}, {0.0f, 1.0f}},
	};
	uint16_t indices[] = { 0, 1, 2, 0, 2, 3, };

	ComPtr<ID3D12Resource> vtxBufUpload{};
	{
		m_numVtx = sizeof(cubeVertices) / sizeof(cubeVertices[0]);
		// vertex buffer
		CD3DX12_HEAP_PROPERTIES vtxHeapPropsGPU		(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_HEAP_PROPERTIES vtxHeapPropsUpload	(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC   vtxBufDesc			= CD3DX12_RESOURCE_DESC::Buffer(m_numVtx * sizeof(Vertex));
		// GPU용 버텍스 버퍼
		hr = d3dDevice->CreateCommittedResource(&vtxHeapPropsGPU, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_rscVtxGPU));
		if (FAILED(hr))
			return hr;
		// CPU 업로드 버퍼
		hr = d3dDevice->CreateCommittedResource(&vtxHeapPropsUpload, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vtxBufUpload));
		if (FAILED(hr))
			return hr;
		// Create vertex/index buffer views.
		m_viewVtx.BufferLocation = m_rscVtxGPU->GetGPUVirtualAddress();
		m_viewVtx.StrideInBytes = sizeof(Vertex);
		m_viewVtx.SizeInBytes = sizeof(cubeVertices);
		m_numIdx = sizeof(indices) / sizeof(indices[0]);
	}

	ComPtr<ID3D12Resource> idxBufUpload{};
	{
		// index buffer
		CD3DX12_HEAP_PROPERTIES idxHeapPropsGPU		(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_HEAP_PROPERTIES idxHeapPropsUpload	(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC	idxBufDesc			= CD3DX12_RESOURCE_DESC::Buffer(m_numIdx * sizeof(uint16_t));
		// GPU용 인덱스 버퍼
		hr = d3dDevice->CreateCommittedResource(&idxHeapPropsGPU, D3D12_HEAP_FLAG_NONE, &idxBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_rscIdxGPU));
		if (FAILED(hr))
			return hr;
		// CPU 업로드 버퍼
		hr = d3dDevice->CreateCommittedResource(&idxHeapPropsUpload, D3D12_HEAP_FLAG_NONE, &idxBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&idxBufUpload));
		if (FAILED(hr))
			return hr;
		// Create vertex/index buffer views.
		m_viewIdx.BufferLocation = m_rscIdxGPU->GetGPUVirtualAddress();
		m_viewIdx.SizeInBytes = sizeof(indices);
		m_viewIdx.Format = DXGI_FORMAT_R16_UINT;
	}

	hr = commandList->Reset(commandAlloc, nullptr);  // PSO는 루프 내에서 설정 예정
	if(FAILED(hr))
		return hr;
	{
		// 업로드 수행
		// upload 요청하면 완료될때까지 모든 데이터가 유효해야 한다.
		{
			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData      = reinterpret_cast<const BYTE*>(cubeVertices);
			vertexData.RowPitch   = m_numVtx * sizeof(Vertex);
			vertexData.SlicePitch = m_numVtx * sizeof(Vertex);
			UpdateSubresources(commandList, m_rscVtxGPU.Get(), vtxBufUpload.Get(), 0, 0, 1, &vertexData);
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_rscVtxGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			commandList->ResourceBarrier(1, &barrier);
		}

			// 업로드 수행
		// upload 요청하면 완료될때까지 모든 데이터가 유효해야 한다.
		{
			D3D12_SUBRESOURCE_DATA indexData = {};
			indexData.pData      = reinterpret_cast<const BYTE*>(indices);
			indexData.RowPitch   = m_numIdx * sizeof(uint16_t);
			indexData.SlicePitch = m_numIdx * sizeof(uint16_t);
			UpdateSubresources(commandList, m_rscIdxGPU.Get(), idxBufUpload.Get(), 0, 0, 1, &indexData);
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_rscIdxGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
			commandList->ResourceBarrier(1, &barrier);
		}
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


	// 이들은 업로드 완료 까지 유효해야 함. 중간에 
	//Vertex cubeVertices
	//uint16_t indices
	//ComPtr<ID3D12Resource> vtxBufUpload
	//ComPtr<ID3D12Resource> idxBufUpload

	// gpu 완료 될 때 까지 기다림
	D3DApp::getInstance()-> WaitForGpu();
}

int MainApp::InitSpine()
{
	spine::Bone::setYDown(false);

	m_spineAtlas = new spine::Atlas("assets/spine/raptor/raptor-pma.atlas",this);
	spine::SkeletonJson binary(m_spineAtlas);
	m_spineSkeletonData = binary.readSkeletonDataFile("assets/spine/raptor/raptor-pro.json");

	m_spineSkeleton = new spine::Skeleton(m_spineSkeletonData);
	m_spineSkeleton->setPosition(0.0F, -300.0F);
	m_spineSkeleton->setScaleX(0.6f);
	m_spineSkeleton->setScaleY(0.6f);

	spine::SkeletonData* skelData = m_spineSkeleton->getData();
	auto& animations = skelData->getAnimations();

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
			size_t vtxCount = mesh->getWorldVerticesLength()/2;
			if(vtxCount> maxVertexCount)
				maxVertexCount = vtxCount;

			size_t indexCount = mesh->getTriangles().size();
			if(indexCount>maxIndexCount)
				maxIndexCount = indexCount;
		}
	}

	m_bufVtxCount = (maxVertexCount > 8)? maxVertexCount: 8;
	m_bufIdxCount = maxIndexCount >8? maxIndexCount : 8;

	// TODO
	// 3. Create vertex buffer
	{
		// position float2
		// m_bufVtxPos
		// diffuse  uint32
		// m_bufVtxDif
		// texcoord float2
		// m_bufVtxTex
	}
	// 인덱스 버퍼
	{
		// uint16_t
	}

	// spine 생성
	spine::AnimationStateData animationStateData(m_spineSkeletonData);
	animationStateData.setDefaultMix(0.2f);
	m_spineAniState = new spine::AnimationState(&animationStateData);
	m_spineAniState->setAnimation(0,"gun-holster", false);
	m_spineAniState->addAnimation(0,"roar",false, 0.8F);
	m_spineAniState->addAnimation(0,"walk",true, 2.1F);

	return S_OK;
}