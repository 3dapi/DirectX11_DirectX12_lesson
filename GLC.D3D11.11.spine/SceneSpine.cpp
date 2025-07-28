// for avx2 memcpy
#include <immintrin.h>
#include <cstdint>
#include <cstring>

#include <any>
#include <filesystem>
#include <memory>
#include <vector>
#include <Windows.h>
#include <d3d11.h>
#include <DirectxColors.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <wrl.h>
#include <shellapi.h>
#include <WICTextureLoader.h>

#include "d3dx11.h"
#include "G2Base.h"
#include "SceneSpine.h"
#include "G2Util.h"
#include "D3DApp.h"
#include "GameTimer.h"

using namespace DirectX;
using namespace spine;
using namespace G2;


DRAW_BUFFER::~DRAW_BUFFER()
{
	numVb		= {};
	numIb		= {};
	SAFE_RELEASE(vbvPos	);
	SAFE_RELEASE(vbvDif	);
	SAFE_RELEASE(vbvTex	);
	SAFE_RELEASE(ibv	);
}

int DRAW_BUFFER::Setup(ID3D11Device* d3dDevice, UINT countVtx, UINT countIdx)
{
	int hr = S_OK;
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(XMFLOAT2) * countVtx;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = d3dDevice->CreateBuffer(&bd, nullptr, &vbvPos);
		if(FAILED(hr))
			return hr;
	}
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(uint32_t) * countVtx;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = d3dDevice->CreateBuffer(&bd, nullptr, &vbvDif);
		if(FAILED(hr))
			return hr;
	}
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(XMFLOAT2) * countVtx;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = d3dDevice->CreateBuffer(&bd, nullptr, &vbvTex);
		if(FAILED(hr))
			return hr;
	}
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(uint16_t) * countIdx;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = d3dDevice->CreateBuffer(&bd, nullptr, &ibv);
		if(FAILED(hr))
			return hr;
	}
	return S_OK;
}


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
	SAFE_RELEASE(	m_shaderVtx	);
	SAFE_RELEASE(	m_shaderPxl	);
	SAFE_RELEASE(	m_vtxLayout	);

	m_maxVtxCount	= {};
	m_maxIdxCount	= {};
	m_drawBuf.clear();

	SAFE_RELEASE(m_cnstMVP		);
	SAFE_RELEASE(m_srvTexture	);
	SAFE_RELEASE(m_sampLinear	);
	SAFE_RELEASE(m_rsAlpha	);
	SAFE_RELEASE(m_rsZwrite	);
	SAFE_RELEASE(m_rsZtest	);

	return S_OK;
}

int SceneSpine::Update(float deltaTime)
{
	//deltaTime = 0;
	float aspectRatio  = 1280/640.0F;
	XMMATRIX tmPrj = XMMatrixPerspectiveFovLH(XM_PIDIV4 * 1.2F, aspectRatio, 1.0f, 5000.0f);
	static const XMVECTORF32 eye = {0.0f, 10.0f, -900.0f, 0.0f};
	static const XMVECTORF32 at = {0.0f, 0.0f, 0.0f, 0.0f};
	static const XMVECTORF32 up = {0.0f, 1.0f, 0.0f, 0.0f};
	XMMATRIX tmViw = XMMatrixLookAtLH(eye, at, up);
	XMMATRIX tmWld = XMMatrixIdentity();
	m_mtMVP = tmWld * tmViw * tmPrj;

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
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

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


	return S_OK;
}

int SceneSpine::UpdateDrawBuffer()
{
	int hr = S_OK;
	auto d3d        = IG2GraphicsD3D::getInstance();
	auto d3dDevice  = std::any_cast<ID3D11Device*           >(d3d->GetDevice());
	auto d3dContext = std::any_cast<ID3D11DeviceContext*    >(d3d->GetContext());

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
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->vbvPos, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				float* ptrPos = reinterpret_cast<float*>(mapped.pData);
				meshAttachment->computeWorldVertices(*slot, 0, meshAttachment->getWorldVerticesLength(), ptrPos, 0, 2);
				d3dContext->Unmap(buf->vbvPos, 0);
			}
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->vbvTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				float* ptrTex = reinterpret_cast<float*>(mapped.pData);
				avx2_memcpy(ptrTex, spineTex, texSize);
				d3dContext->Unmap(buf->vbvTex, 0);
			}
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->vbvDif, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				uint32_t* ptrDif = reinterpret_cast<uint32_t*>(mapped.pData);
				avx2_memset32(ptrDif, rgba, vtxCount);
				d3dContext->Unmap(buf->vbvDif, 0);
			}
			// Index
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->ibv, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				uint16_t* ptrIdx = reinterpret_cast<uint16_t*>(mapped.pData);
				avx2_memcpy(ptrIdx, spineIdx, idxSize);
				d3dContext->Unmap(buf->ibv, 0);
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

			// Upload → GPU 복사 (Position)
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->vbvPos, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				float* ptrPos = reinterpret_cast<float*>(mapped.pData);
				regionAttachment->computeWorldVertices(*slot, (float*)ptrPos, 0, 2);
				d3dContext->Unmap(buf->vbvPos, 0);
			}
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->vbvTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				float* ptrTex = reinterpret_cast<float*>(mapped.pData);
				avx2_memcpy(ptrTex, spineTex, texSize);
				d3dContext->Unmap(buf->vbvTex, 0);
			}
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->vbvDif, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				uint32_t* ptrDif = reinterpret_cast<uint32_t*>(mapped.pData);
				avx2_memset32(ptrDif, rgba, vtxCount);
				d3dContext->Unmap(buf->vbvDif, 0);
			}
			// Index
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				hr = d3dContext->Map(buf->ibv, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if(FAILED(hr))
					return hr;
				uint16_t* ptrIdx = reinterpret_cast<uint16_t*>(mapped.pData);
				avx2_memcpy(ptrIdx, spineIdx, idxSize);
				d3dContext->Unmap(buf->ibv, 0);
			}
		}

		// Bind
		buf->numVb  = vtxCount;
		buf->numIb  = idxCount;
	}

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

	m_spineAniStateData = new AnimationStateData(m_spineSkeletonData);
	m_spineAniState = new AnimationState(m_spineAniStateData);

	m_spineAniStateData->setDefaultMix(0.2f);
	//m_spineAniState->setAnimation(0, "gun-holster", false);
	//m_spineAniState->addAnimation(0, "roar", false, 0.8F);
	m_spineAniState->addAnimation(0, "walk", true, 0);

	m_spineSkeleton->setPosition(0.0F, -300.0F);
	m_spineSkeleton->setScaleX(0.6f);
	m_spineSkeleton->setScaleY(0.6f);

	auto d3dDevice  = std::any_cast<ID3D11Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	for(size_t i=0; i<m_drawBuf.size(); ++i)
	{
		m_drawBuf[i].Setup(d3dDevice, m_maxVtxCount, m_maxIdxCount);
	}

	return S_OK;
}

int SceneSpine::InitForDevice()
{
	HRESULT hr = S_OK;
	auto d3dDevice  = std::any_cast<ID3D11Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

	// create vertex shader
	// 1.1 Compile the vertex shader
	ID3DBlob* pBlobVtx{};
	{
		hr = G2::DXCompileShaderFromFile("assets/shader/spine.hlsl", "vs_4_0", "main_vs", &pBlobVtx);
		if (FAILED(hr))
		{
			MessageBox({}, "Compile Failed. ", "Error", MB_OK);
			return hr;
		}
		hr = d3dDevice->CreateVertexShader(pBlobVtx->GetBufferPointer(), pBlobVtx->GetBufferSize(), {}, &m_shaderVtx);
		if(FAILED(hr))
			return hr;

	}
	// 1.2 Compile the pixel shader
	{
		ID3DBlob* pBlob{};
		hr = G2::DXCompileShaderFromFile("assets/shader/spine.hlsl", "ps_4_0", "main_ps", &pBlob);
		if (FAILED(hr))
		{
			MessageBox({}, "Compile Failed. ", "Error", MB_OK);
			return hr;
		}
		hr = d3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), {}, &m_shaderPxl);
		SAFE_RELEASE(pBlob);
		if(FAILED(hr))
			return hr;
	}

	// 1.3 create vertexLayout
	// Define the input layout
	vector<D3D11_INPUT_ELEMENT_DESC> layout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT    , 0, 0 , D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM  , 1, 0 , D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT    , 2, 0 , D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	
	hr = d3dDevice->CreateInputLayout((D3D11_INPUT_ELEMENT_DESC*)layout.data(), (UINT)layout.size()
										, pBlobVtx->GetBufferPointer(), pBlobVtx->GetBufferSize(), &m_vtxLayout);
	SAFE_RELEASE(pBlobVtx);
	if(FAILED(hr))
		return hr;

	// 2. Create the constant buffer
	// MVP constant
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(XMMATRIX);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstMVP);
		if(FAILED(hr))
			return hr;
	}
	// 3. Create sampler
	{
		D3D11_SAMPLER_DESC sampDesc = {};
			sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			sampDesc.MinLOD = 0;
			sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		hr = d3dDevice->CreateSamplerState(&sampDesc, &m_sampLinear);
		if(FAILED(hr))
			return hr;
	}

	// alpha blending option
	{
		D3D11_BLEND_DESC blendDesc = {};
			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = d3dDevice->CreateBlendState(&blendDesc, &m_rsAlpha);
		if(FAILED(hr))
			return hr;
	}
	// z-write false
	{
		D3D11_DEPTH_STENCIL_DESC dssDesc = {};
			dssDesc.DepthEnable = TRUE;
			dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
			dssDesc.StencilEnable = FALSE;
		hr = d3dDevice->CreateDepthStencilState(&dssDesc, &m_rsZwrite);
		if(FAILED(hr))
			return hr;
	}
	// z-test false
	{
		D3D11_DEPTH_STENCIL_DESC dssDesc = {};
			dssDesc.DepthEnable = FALSE;
			dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			dssDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
			dssDesc.StencilEnable = FALSE;
		hr = d3dDevice->CreateDepthStencilState(&dssDesc, &m_rsZtest);
		if(FAILED(hr))
			return hr;
	}

	return S_OK;
}

void* SceneSpine::TextureLoad(const string& fileName)
{
	m_spineTextureName = fileName;
	int hr = S_OK;
	std::tie(hr, m_srvTexture, std::ignore) = G2::DXCreateTextureFromFile(m_spineTextureName);
	if(FAILED(hr))
		return nullptr;
	return m_srvTexture;
}

void SceneSpine::TextureUnload(void* texture)
{
	auto textureRsc = m_srvTexture;
	if(texture != textureRsc)
	{
		// bad texture resource pointer
		//
		return;
	}
	SAFE_RELEASE(m_srvTexture);
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
