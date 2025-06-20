#include <any>
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

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 p;
	XMFLOAT4 d;
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
		ComPtr<ID3DBlob> shaderVtx{};
		ComPtr<ID3DBlob> shaderPxl{};
		hr = G2::DXCompileShaderFromFile("assets/simple.hlsl", "main_vx", "vs_5_0", &shaderVtx);
		if (FAILED(hr))
			return hr;
		hr = G2::DXCompileShaderFromFile("assets/simple.hlsl", "main_ps", "ps_5_0", &shaderPxl);
		if (FAILED(hr))
			return hr;
		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
			psoDesc.pRootSignature = d3dRootSignature;
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
		psoDesc.RasterizerState = rasterDesc;

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
		psoDesc.BlendState = blendDesc;

		hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
		if (FAILED(hr))
			return hr;
	}

	// 4. Create the vertex buffer.
	{
		const UINT vertexBufferSize = (sizeof(XMFLOAT3) + sizeof(XMFLOAT4))  * 6;

		D3D12_HEAP_PROPERTIES heapProps = {};
			heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProps.CreationNodeMask = 1;
			heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = vertexBufferSize;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		hr = d3dDevice->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_rscVtx));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
		hr = m_rscVtx->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
		if (FAILED(hr))
			return hr;

		// memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		auto screenSize = std::any_cast<::SIZE*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_CMD::ATTRIB_SCREEN_SIZE));
		float aspectRatio = screenSize->cx / (float)(screenSize->cy);
		// D3D12_HEAP_TYPE_UPLOAD: CPU -> GPU라 가능
		Vertex* pCur = reinterpret_cast<Vertex*>(pVertexDataBegin);
		*pCur++ = { { -0.25f,  0.25f * aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } };
		*pCur++ = { {  0.25f,  0.25f * aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } };
		*pCur++ = { {  0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } };
		*pCur++ = { { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 0.5f, 0.5f, 1.0f } };

		// Unmap은 안해도 됨
		//hr = m_rscVtx->Unmap(0, nullptr);
		//if (FAILED(hr))
		//	return hr;

		// Initialize the vertex buffer view.
		m_vtxView.BufferLocation = m_rscVtx->GetGPUVirtualAddress();
		m_vtxView.StrideInBytes = sizeof(Vertex);
		m_vtxView.SizeInBytes = vertexBufferSize;
	}

	return S_OK;
}

int MainApp::Destroy()
{
	m_pipelineState.Reset();
	return S_OK;
}

int MainApp::Update()
{
	return S_OK;
}

int MainApp::Render()
{
	auto d3dCommandList = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	d3dCommandList->SetPipelineState(m_pipelineState.Get());
	d3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN);
	d3dCommandList->IASetVertexBuffers(0, 1, &m_vtxView);
	d3dCommandList->DrawInstanced(4, 1, 0, 0);
	return S_OK;
}

