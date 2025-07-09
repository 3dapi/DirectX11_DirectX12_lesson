// for avx2 memcpy
#include <immintrin.h>
#include <cstdint>
#include <cstring>

#include <any>
#include <filesystem>
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
#include "SceneSpine.h"
#include "G2Util.h"
#include "D3DApp.h"
#include "GameTimer.h"

using namespace DirectX;
using namespace spine;
using namespace G2;

SceneSpine::SceneSpine()
{
}

SceneSpine::~SceneSpine()
{
	Destroy();
}

int SceneSpine::Init()
{
	HRESULT hr = S_OK;

	hr = InitSpine("assets/spine/spineboy/spineboy-pma.atlas", "assets/spine/spineboy/spineboy-pro.json");
	if(FAILED(hr))
		return hr;
	hr = InitForDevice();
	if(FAILED(hr))
		return hr;
	return S_OK;
}

int SceneSpine::Destroy()
{
	m_cbvHeap		.Reset();
	m_rootSignature	.Reset();
	m_pipelineState	.Reset();

	m_maxVtxCount	= {};
	m_maxIdxCount	= {};
	m_cbvHandle		= {};

	m_cnstMVP		->Unmap(0, nullptr);
	m_cnstMVP		.Reset();
	m_ptrMVP		= {};
	m_spineTextureRsc	.Reset();
	m_spineTextureHandle	= {};

	return S_OK;
}

int SceneSpine::Update(float deltaTime)
{
	//deltaTime = 0;
	auto d3dDevice = any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	float aspectRatio  = 1280/640.0F;
	XMMATRIX tmPrj = XMMatrixPerspectiveFovLH(XM_PIDIV4 * 1.2F, aspectRatio, 1.0f, 5000.0f);
	static const XMVECTORF32 eye = {0.0f, 10.0f, -900.0f, 0.0f};
	static const XMVECTORF32 at = {0.0f, 0.0f, 0.0f, 0.0f};
	static const XMVECTORF32 up = {0.0f, 1.0f, 0.0f, 0.0f};
	XMMATRIX tmViw = XMMatrixLookAtLH(eye, at, up);
	XMMATRIX tmWld = XMMatrixIdentity();
	XMMATRIX mtMVP = tmWld * tmViw * tmPrj;

	auto currentFrameIndex = *(any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));
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
	m_spineSkeleton->updateWorldTransform(spine::Physics_None);

	// Update spine draw buffer
	UpdateDrawBuffer();

	return S_OK;
}

int SceneSpine::Render()
{
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto cmdList = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	UINT frameIndex = *std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX));

	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
	ID3D12DescriptorHeap* descriptorHeaps[] = {m_cbvHeap.Get()};
	cmdList->SetDescriptorHeaps(1, descriptorHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), frameIndex, m_descriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
	cmdList->SetGraphicsRootDescriptorTable(1, m_spineTextureHandle);
	cmdList->SetPipelineState(m_pipelineState.Get());

	for(int i=0; i<m_drawCount; ++i)
	{
		auto& buf = m_drawBuf[i];
		cmdList->IASetVertexBuffers(0, 1, &buf.vbvPos);
		cmdList->IASetVertexBuffers(1, 1, &buf.vbvDif);
		cmdList->IASetVertexBuffers(2, 1, &buf.vbvTex);
		cmdList->IASetIndexBuffer(&buf.ibv);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawIndexedInstanced(buf.ibv.SizeInBytes/sizeof(uint16_t), 1, 0, 0, 0);
	}

	return S_OK;
}

int SceneSpine::UpdateDrawBuffer()
{
	int hr = S_OK;
	auto d3d        = IG2GraphicsD3D::getInstance();
	auto d3dDevice  = std::any_cast<ID3D12Device*               >(d3d->GetDevice());
	auto cmdAlloc   = std::any_cast<ID3D12CommandAllocator*     >(d3d->GetCommandAllocator());
	auto cmdList    = std::any_cast<ID3D12GraphicsCommandList*  >(d3d->GetCommandList());
	auto cmdQue     = std::any_cast<ID3D12CommandQueue*         >(d3d->GetCommandQueue());

	hr = cmdAlloc->Reset();
	if(FAILED(hr))
		return hr;
	hr = cmdList->Reset(cmdAlloc, nullptr);
	if(FAILED(hr))
		return hr;
	
	m_drawCount = {};
	const auto& drawOrder = m_spineSkeleton->getDrawOrder();
	for(size_t i = 0; i < drawOrder.size(); ++i)
	{
		auto* slot = drawOrder[i];
		auto* attachment = slot->getAttachment();
		if(!attachment)
			continue;

		DRAW_BUFFER*		buf  = {};
		MeshAttachment*		meshAttachment   = {};
		RegionAttachment*	regionAttachment = {};

		auto isMesh = attachment->getRTTI().isExactly(spine::MeshAttachment::rtti);
		auto isResion = attachment->getRTTI().isExactly(spine::RegionAttachment::rtti);
		if (!(isMesh || isResion))
			continue;

		if (isMesh)
		{
			auto* mesh = static_cast<spine::MeshAttachment*>(attachment);
			auto* texRegion = mesh->getRegion();
			if (!texRegion)
				continue;
			meshAttachment = mesh;
		}
		else if (isResion)
		{
			auto* region = static_cast<spine::RegionAttachment*>(attachment);
			auto* texRegion = region->getRegion();
			if (!texRegion)
				continue;
			regionAttachment = region;
		}

		buf = &m_drawBuf[m_drawCount++];

		uint32_t    rgba        = 0xFFFFFFFF;
		UINT        idxCount    = {};
		UINT        vtxCount    = {};
		UINT        posSize     = {};
		UINT        difSize     = {};
		UINT        texSize     = {};
		UINT        idxSize     = {};

		if(isMesh)
		{
			float*    spineTex = const_cast<float*>(meshAttachment->getUVs().buffer());
			uint16_t* spineIdx = meshAttachment->getTriangles().buffer();
			
			vtxCount    = (UINT)meshAttachment->getWorldVerticesLength() / 2;
			idxCount    = (UINT)meshAttachment->getTriangles().size();

			spine::Color c = slot->getColor();
			spine::Color m = meshAttachment->getColor();
			rgba =
				((uint32_t)(c.a * m.a * 255) << 24) |
				((uint32_t)(c.r * m.r * 255) << 16) |
				((uint32_t)(c.g * m.g * 255) << 8) |
				((uint32_t)(c.b * m.b * 255) << 0);

			posSize = sizeof(XMFLOAT2) * vtxCount;
			difSize = sizeof(uint32_t) * vtxCount;
			texSize = sizeof(XMFLOAT2) * vtxCount;
			idxSize = sizeof(uint16_t) * idxCount;

			// Upload → GPU 복사 (Position)
			{
				XMFLOAT2* ptrPos = nullptr;
				buf->rscPosCPU->Map(0, nullptr, (void**)&ptrPos);
				meshAttachment->computeWorldVertices(*slot, 0, meshAttachment->getWorldVerticesLength(), (float*)ptrPos, 0, 2);
				buf->rscPosCPU->Unmap(0, nullptr);
			}
			// Upload → GPU 복사 (texture coord)
			{
				XMFLOAT2* ptrTex = nullptr;
				buf->rscTexCPU->Map(0, nullptr, (void**)&ptrTex);
				avx2_memcpy(ptrTex, spineTex, texSize);
				buf->rscTexCPU->Unmap(0, nullptr);
			}
			// Upload → GPU 복사 (diffuse)
			{
				uint32_t* ptrDif = nullptr;
				buf->rscDifCPU->Map(0, nullptr, (void**)&ptrDif);
				avx2_memset32(ptrDif, rgba, vtxCount);
				buf->rscDifCPU->Unmap(0, nullptr);
			}
			// Index
			{
				uint16_t* ptrIdx = nullptr;
				buf->rscIdxCPU->Map(0, nullptr, (void**)&ptrIdx);
				avx2_memcpy(ptrIdx, spineIdx, idxSize);
				buf->rscIdxCPU->Unmap(0, nullptr);
			}
		}
		else if (isResion)
		{
			static const uint16_t spineIdx[] = { 0, 1, 2, 2, 3, 0 };
			float* spineTex = const_cast<float*>(regionAttachment->getUVs().buffer());

			vtxCount    = 4;
			idxCount    = 6;

			auto c = slot->getColor();
			auto r = regionAttachment->getColor();
			rgba =
				((uint32_t)(c.a * r.a * 255) << 24) |
				((uint32_t)(c.r * r.r * 255) << 16) |
				((uint32_t)(c.g * r.g * 255) << 8) |
				((uint32_t)(c.b * r.b * 255) << 0);

			posSize = sizeof(XMFLOAT2) * vtxCount;
			difSize = sizeof(uint32_t) * vtxCount;
			texSize = sizeof(XMFLOAT2) * vtxCount;
			idxSize = sizeof(uint16_t) * idxCount;

			// Position
			{
				XMFLOAT2* ptrPos = nullptr;
				buf->rscPosCPU->Map(0, nullptr, (void**)&ptrPos);
				regionAttachment->computeWorldVertices(*slot, (float*)ptrPos, 0, 2);
				buf->rscPosCPU->Unmap(0, nullptr);
			}
			// texture coord
			{
				XMFLOAT2* ptrTex = nullptr;
				buf->rscTexCPU->Map(0, nullptr, (void**)&ptrTex);
				avx2_memcpy(ptrTex, spineTex, texSize);
				buf->rscTexCPU->Unmap(0, nullptr);
			}
			// diffuse
			{
				uint32_t* ptrDif = nullptr;
				buf->rscDifCPU->Map(0, nullptr, (void**)&ptrDif);
				avx2_memset32(ptrDif, rgba, vtxCount);
				buf->rscDifCPU->Unmap(0, nullptr);
			}
			// Index
			{
				uint16_t* ptrIdx = nullptr;
				buf->rscIdxCPU->Map(0, nullptr, (void**)&ptrIdx);
				avx2_memcpy(ptrIdx, spineIdx, idxSize);
				buf->rscIdxCPU->Unmap(0, nullptr);
			}
		}

		cmdList->CopyBufferRegion(buf->rscPosGPU.Get(), 0, buf->rscPosCPU.Get(), 0, posSize);
		CD3DX12_RESOURCE_BARRIER barPos = CD3DX12_RESOURCE_BARRIER::Transition(buf->rscPosGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		cmdList->ResourceBarrier(1, &barPos);

		cmdList->CopyBufferRegion(buf->rscDifGPU.Get(), 0, buf->rscDifCPU.Get(), 0, difSize);
		CD3DX12_RESOURCE_BARRIER barDif = CD3DX12_RESOURCE_BARRIER::Transition(buf->rscDifGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		cmdList->ResourceBarrier(1, &barDif);

		cmdList->CopyBufferRegion(buf->rscTexGPU.Get(), 0, buf->rscTexCPU.Get(), 0, texSize);
		CD3DX12_RESOURCE_BARRIER barTex = CD3DX12_RESOURCE_BARRIER::Transition(buf->rscTexGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		cmdList->ResourceBarrier(1, &barTex);

		cmdList->CopyBufferRegion(buf->rscIdxGPU.Get(), 0, buf->rscIdxCPU.Get(), 0, idxSize);
		CD3DX12_RESOURCE_BARRIER barIdx = CD3DX12_RESOURCE_BARRIER::Transition(buf->rscIdxGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		cmdList->ResourceBarrier(1, &barIdx);

		// Bind
		buf->numVb  = vtxCount;
		buf->numIb  = idxCount;
		buf->vbvPos = {buf->rscPosGPU->GetGPUVirtualAddress(), posSize, sizeof(XMFLOAT2)};
		buf->vbvDif = {buf->rscDifGPU->GetGPUVirtualAddress(), difSize, sizeof(uint32_t)};
		buf->vbvTex = {buf->rscTexGPU->GetGPUVirtualAddress(), texSize, sizeof(XMFLOAT2)};
		buf->ibv    = {buf->rscIdxGPU->GetGPUVirtualAddress(), idxSize, DXGI_FORMAT_R16_UINT };
	}

	hr = cmdList->Close();
	if(FAILED(hr))
		return hr;
	ID3D12CommandList* ppCmdLists[] = {cmdList};
	cmdQue->ExecuteCommandLists(_countof(ppCmdLists), ppCmdLists);
	d3d->WaitForGpu();
	return S_OK;
}

int SceneSpine::InitSpine(const string& str_atlas, const string& str_skel)
{
	Bone::setYDown(false);

	filesystem::path str_path(str_skel);
	m_spineAtlas = new Atlas(str_atlas.c_str(),this);
	if(str_path.extension().generic_string().compare("json"))
	{
		SkeletonJson skl(m_spineAtlas);
		m_spineSkeletonData = skl.readSkeletonDataFile(str_skel.c_str());
	}
	else
	{
		SkeletonBinary skl(m_spineAtlas);
		m_spineSkeletonData = skl.readSkeletonDataFile(str_skel.c_str());
	}

	m_spineSkeleton = new Skeleton(m_spineSkeletonData);
	spine::SkeletonData* skelData = m_spineSkeleton->getData();


	m_spineSkeleton->setSkin("default");		// 스킨 변경
	m_spineSkeleton->setSlotsToSetupPose();						// 슬롯(attachment) 갱신
	m_spineSkeleton->updateWorldTransform(spine::Physics_None);	// 본 transform 계산



	auto& animations = skelData->getAnimations();
	// animation name
	for(int i = 0; i < animations.size(); ++i) {
		spine::Animation* anim = animations[i];
		string animName = anim->getName().buffer();
		m_spineAnimations.push_back(animName);
	}

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

			size_t idxCount = mesh->getTriangles().size();
			if(idxCount > maxIndexCount)
				maxIndexCount = idxCount;
		}
	}
	// buffer 최댓값으로 설정.
	m_maxVtxCount = UINT( (maxVertexCount>8) ? maxVertexCount : 8 );
	m_maxIdxCount = UINT( (maxIndexCount>8) ? maxIndexCount : 8 );
	m_drawBuf.resize(size_t(3+ drawOrder.size() * 1.2), {});

	AnimationStateData animationStateData(m_spineSkeletonData);
	animationStateData.setDefaultMix(0.2f);
	m_spineAniState = new AnimationState(&animationStateData);
	//m_spineAniState->setAnimation(0, "gun-holster", false);
	//m_spineAniState->addAnimation(0, "roar", false, 0.8F);
	m_spineAniState->addAnimation(0, "walk", true, 0);

	m_spineSkeleton->setPosition(0.0F, -300.0F);
	m_spineSkeleton->setScaleX(0.6f);
	m_spineSkeleton->setScaleY(0.6f);

	return S_OK;
}

int SceneSpine::InitForDevice()
{
	HRESULT hr = S_OK;
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());

	m_descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
		heapDesc.NumDescriptors = FRAME_BUFFER_COUNT * NUM_CB  + NUM_TX;	// 프레임당 상수 버퍼 1개 (b0) * 프레임 수 + 텍스처용 SRV(t0) 1개
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
			{ 2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
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
			4,					// sampler register 숫자.
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
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT    , 0, 0 , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM  , 1, 0 , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT    , 2, 0 , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(shaderVtx.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(shaderPxl.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		// 추가 코드:알파 블렌딩 켜고 src-alpha inv-src-alpha
		psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		// 추가 코드: Z-write, Z-test 끄기:
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.StencilEnable = FALSE;

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
	// 5 텍스처 생성 및 업로드
	{
		//spine load에서 처리함
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
			m_spineTextureHandle = hGpuSrv;					// CPU, GPU OFFSET을 이동후 Heap pointer 위치를 저장 이 핸들 값이 텍스처 핸들

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = m_spineTextureRsc->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			d3dDevice->CreateShaderResourceView(m_spineTextureRsc.Get(), &srvDesc, hCpuSrv);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 7. vertex, index buffer uploader 생성
	const UINT widthVertex = m_maxVtxCount * sizeof(XMFLOAT2);
	const UINT widthIndex  = m_maxIdxCount * sizeof(uint16_t);
	CD3DX12_HEAP_PROPERTIES heapPropsGPU	(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_HEAP_PROPERTIES heapPropsUpload	(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC   vtxBufDesc		= CD3DX12_RESOURCE_DESC::Buffer(widthVertex);
	CD3DX12_RESOURCE_DESC	idxBufDesc		= CD3DX12_RESOURCE_DESC::Buffer(widthIndex);
	for(size_t i=0; i<m_drawBuf.size(); ++i)
	{
		m_drawBuf[i].Setup(d3dDevice, widthVertex, widthIndex, heapPropsGPU, heapPropsUpload, vtxBufDesc, idxBufDesc);
	}

	D3DApp::getInstance()-> WaitForGpu();

	return S_OK;
}

void* SceneSpine::TextureLoad(const string& fileName)
{
	m_spineTextureName = fileName;
	auto d3dDevice = any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	DirectX::ResourceUploadBatch resourceUpload(d3dDevice);
	{
		resourceUpload.Begin();
		auto wFile = ansiToWstr(fileName);
		int hr = DirectX::CreateWICTextureFromFile(d3dDevice, resourceUpload, wFile.c_str(), m_spineTextureRsc.GetAddressOf());
		if(FAILED(hr))
			return {};
		auto commandQueue = any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
		auto uploadOp = resourceUpload.End(commandQueue);
		uploadOp.wait();  // GPU 업로드 완료 대기
	}
	// 뷰는 descriptor heap 생성 후에 만듦.
	return m_spineTextureRsc.Get();
}

void SceneSpine::TextureUnload(void* texture)
{
	auto textureRsc = m_spineTextureRsc.Get();
	if(texture != textureRsc)
	{
		// bad texture resource pointer
		//
	}
}

namespace spine {
	spine::SpineExtension* getDefaultExtension()
	{
		static spine::SpineExtension* _default_spineExtension = new spine::DefaultSpineExtension;
		return _default_spineExtension;
	}
}

void SceneSpine::load(spine::AtlasPage& page, const spine::String& path) {
	auto fileName = path.buffer();
	page.texture = this->TextureLoad(fileName);
}

void SceneSpine::unload(void* texture) {
	this->TextureUnload(texture);
}

DRAW_BUFFER::~DRAW_BUFFER()
{
	rscPosGPU	.Reset();
	rscPosCPU	.Reset();
	vbvPos		= {};

	rscDifGPU	.Reset();
	rscDifCPU	.Reset();
	vbvDif		= {};

	rscTexGPU	.Reset();
	rscTexCPU	.Reset();
	vbvTex		= {};

	rscIdxGPU	.Reset();
	rscIdxCPU	.Reset();
	ibv			= {};
}

int DRAW_BUFFER::Setup(ID3D12Device* d3dDevice, UINT widthVertex, UINT widthIndex
				, const CD3DX12_HEAP_PROPERTIES& heapPropsGPU, const CD3DX12_HEAP_PROPERTIES& heapPropsUpload
				, const CD3DX12_RESOURCE_DESC& vtxBufDesc, const CD3DX12_RESOURCE_DESC& idxBufDesc)
{
	int hr = S_OK;
	// GPU position buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsGPU, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rscPosGPU));
	if(FAILED(hr))
		return hr;
	// CPU upload position buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsUpload, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rscPosCPU));
	if(FAILED(hr))
		return hr;
	// GPU diffuse buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsGPU, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rscDifGPU));
	if(FAILED(hr))
		return hr;
	// CPU upload diffuse buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsUpload, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rscDifCPU));
	if(FAILED(hr))
		return hr;
	// GPU texture coord buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsGPU, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rscTexGPU));
	if(FAILED(hr))
		return hr;
	// CPU upload texture coord buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsUpload, D3D12_HEAP_FLAG_NONE, &vtxBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rscTexCPU));
	if(FAILED(hr))
		return hr;
	// GPU index buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsGPU, D3D12_HEAP_FLAG_NONE, &idxBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rscIdxGPU));
	if(FAILED(hr))
		return hr;
	// CPU upload index buffer
	hr = d3dDevice->CreateCommittedResource(&heapPropsUpload, D3D12_HEAP_FLAG_NONE, &idxBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rscIdxCPU));
	if(FAILED(hr))
		return hr;

	return S_OK;
}
