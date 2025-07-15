#include <any>
#include <memory>
#include <vector>
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

using namespace std;

GameTimer	m_timer;

inline CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(ID3D12DescriptorHeap* heap, UINT index, UINT descriptorSize)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(heap->GetGPUDescriptorHandleForHeapStart(), index, descriptorSize);
}
inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(ID3D12DescriptorHeap* heap, UINT index, UINT descriptorSize)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(heap->GetCPUDescriptorHandleForHeapStart(), index, descriptorSize);
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
	if (FAILED(hr))
		return hr;
	hr = SetupResource();
	if (FAILED(hr))
		return hr;
	return S_OK;
}


int MainApp::Destroy()
{
	m_cbvHeap		.Reset();
	m_rootSignature	.Reset();
	m_pipelineState	.Reset();

	m_vtxView		= {};
	m_idxView		= {};
	m_vtxCount		= {};
	m_idxCount		= {};

	m_vtxGPU		.Reset();
	m_idxGPU		.Reset();

	m_cbv0rsc		->Unmap(0, nullptr);
	m_cbv0rsc		.Reset();
	m_cbv0ptr		= {};
	m_textureRsc	.Reset();

	return S_OK;
}

int MainApp::Update()
{
	m_timer.Tick();
	auto deltaTime = m_timer.DeltaTime();

	auto device = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());

	float angle= 0.0F;
	float aspectRatio = 1280.0F / 640.0F;
	XMMATRIX tmPrj = XMMatrixPerspectiveFovLH(XM_PIDIV4 * 1.2F, aspectRatio, 1.0f, 5000.0f);
	static const XMVECTORF32 eye = {0.0f, 10.0f, -900.0f, 0.0f};
	static const XMVECTORF32 at = {0.0f, 0.0f, 0.0f, 0.0f};
	static const XMVECTORF32 up = {0.0f, 1.0f, 0.0f, 0.0f};
	XMMATRIX tmViw = XMMatrixLookAtLH(eye, at, up);
	XMMATRIX tmWld = XMMatrixRotationY((float)angle);
	XMMATRIX mtMVP = tmWld * tmViw * tmPrj;

	auto currentFrameIndex = *(std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));
	{
		uint8_t* dest = m_cbv0ptr + (currentFrameIndex * G2::align256BufferSize(sizeof(XMMATRIX)));
		memcpy(dest, &mtMVP, sizeof(mtMVP));
	}

	return S_OK;
}

int MainApp::Render()
{
	HRESULT hr = S_OK;
	auto device = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto cmdList = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());
	UINT currentFrameIndex = *(std::any_cast<UINT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_CURRENT_FRAME_INDEX)));
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 1. 루트 시그너처
	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());

	// 2. 디스크립터 힙 설정
	ID3D12DescriptorHeap* descriptorHeaps[] = {m_cbvHeap.Get()};
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// 3. CBV 바인딩
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(GetGpuHandle(m_cbvHeap.Get(), currentFrameIndex * 1 + 0, descriptorSize));
	cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

	// 4. SRV 바인딩
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(GetGpuHandle(m_cbvHeap.Get(),  FRAME_BUFFER_COUNT * NUM_CBV + 0, descriptorSize));
	cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);

	// 5. 파이프라인 연결
	cmdList->SetPipelineState(m_pipelineState.Get());


	// 토폴로지
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 버택스, 인덱스 버퍼
	cmdList->IASetVertexBuffers(0, 1, &m_vtxView);
	cmdList->IASetIndexBuffer(&m_idxView);

	// draw
	cmdList->DrawIndexedInstanced(m_idxCount, 1, 0, 0, 0);

	return S_OK;
}


::SIZE MainApp::StringSize(const string& text, HFONT hFont)
{
	HDC hDC = GetDC(nullptr);
	SIZE ret{};
	auto oldFont = (HFONT)SelectObject(hDC, hFont);
	GetTextExtentPoint32(hDC, text.c_str(), (int)text.length(), &ret);
	SelectObject(hDC, oldFont);
	ReleaseDC(nullptr, hDC);
	return ret;
}

int MainApp::InitTexture()
{
	HRESULT hr = S_OK;
	auto device = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto cmdAlloc = std::any_cast<ID3D12CommandAllocator*>(IG2GraphicsD3D::getInstance()->GetCommandAllocator());
	auto cmdList  = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());

	std::string testMessage = "GDI 한글 출력";

	HFONT hFont = CreateFontA(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
							  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
							  DEFAULT_PITCH | FF_DONTCARE, "Arial");

	auto strSize = StringSize(testMessage, hFont);

	// 4의 배수로 맞춤
	const int width  = (strSize.cx +4)/4 * 4;
	const int height = (strSize.cy +4)/4 * 4;
	

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize   = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth  = width;
	bmi.bmiHeader.biHeight = -height; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pBits = nullptr;
	HDC hdcMem = CreateCompatibleDC(nullptr);
	HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	SelectObject(hdcMem, hBitmap);
	memset(pBits, 0x00, width * height * 4); // BGRA → 검정 배경

	auto oldFont = (HFONT)SelectObject(hdcMem, hFont);
	SetBkMode(hdcMem, TRANSPARENT);
	SetTextColor(hdcMem, RGB(255, 255, 255));
	TextOut(hdcMem, 0, 0, testMessage.c_str(), (int)testMessage.size());
	SelectObject(hdcMem, oldFont);

	unique_ptr<uint32_t> pxlBitmap(new uint32_t[width * height]);
	memcpy(pxlBitmap.get(), pBits, width * height*4);

	DeleteObject(hBitmap);
	DeleteObject(hFont);
	DeleteDC(hdcMem);

	auto pixels = pxlBitmap.get();
	for(int y = 0; y < height; ++y)
	{
		for(int x = 0; x < width; ++x)
		{
			auto& px = pixels[y * width + x];
			uint32_t b = (px >> 0) & 0xFF;
			uint32_t g = (px >> 8) & 0xFF;
			uint32_t r = (px >>16) & 0xFF;
			uint32_t a = (b + g + r) /3;
			px |= ((a<<24) & 0xFF000000);
		}
	}

	hr = cmdAlloc->Reset();
	if(FAILED(hr))
		return hr;
	hr = cmdList->Reset(cmdAlloc, nullptr);
	if(FAILED(hr))
		return hr;


	// ✅ D3D12 리소스 생성
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES heapDefault(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_HEAP_PROPERTIES heapUpload(D3D12_HEAP_TYPE_UPLOAD);

	device->CreateCommittedResource(
		&heapDefault,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_textureRsc));

	UINT64 uploadSize = 0;
	device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

	ComPtr<ID3D12Resource> uploadBuf;
	auto bufSize = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
	device->CreateCommittedResource(
		&heapUpload,
		D3D12_HEAP_FLAG_NONE,
		&bufSize,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuf));

	D3D12_SUBRESOURCE_DATA sub = {};
	sub.pData = pxlBitmap.get();
	sub.RowPitch = width * sizeof(uint32_t);
	sub.SlicePitch = width * height * sizeof(uint32_t);

	UpdateSubresources<1>(cmdList, m_textureRsc.Get(), uploadBuf.Get(), 0, 0, 1, &sub);
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_textureRsc.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmdList->ResourceBarrier(1, &barrier);

	hr = cmdList->Close();
	if(FAILED(hr))
		return hr;

	// execute the uploading
	ID3D12CommandList* ppCommandLists[] = {cmdList};
	auto commandQue = std::any_cast<ID3D12CommandQueue*>(IG2GraphicsD3D::getInstance()->GetCommandQueue());
	commandQue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// waiting for ExecuteCommandLists complete
	D3DApp::getInstance()-> WaitForGpu();
	return S_OK;
}


int MainApp::InitDeviceResource()
{
	HRESULT hr = S_OK;
	auto device = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// ★★★★★★★★★★★★★★★
	// 1. 텍스처 생성 및 업로드 (CreateWICTextureFromFile + ResourceUploadBatch)
	InitTexture();

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 2. compile shader
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
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT    , 0, 0                                  , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM  , 0, sizeof(XMFLOAT2)                   , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT    , 0, sizeof(XMFLOAT2) + sizeof(uint32_t), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	//3. Create a root signature with a single constant buffer slot.
	{
		// sampler register 갯수는 상관 없음.
		vector<CD3DX12_STATIC_SAMPLER_DESC> staticSampler =
		{
			{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
			{ 1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP},
		};

		// 2 = 상수 레지스터 1 + 텍스처 레지스터 1
		vector<CD3DX12_DESCRIPTOR_RANGE> descRange{ NUM_CB_TX, CD3DX12_DESCRIPTOR_RANGE{} };
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0
		descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

		vector<CD3DX12_ROOT_PARAMETER> rootParams{ NUM_CB_TX, CD3DX12_ROOT_PARAMETER{} };
		rootParams[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_VERTEX);		// cbv
		rootParams[1].InitAsDescriptorTable(1, &descRange[1], D3D12_SHADER_VISIBILITY_PIXEL);		// src

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init(
			(UINT)rootParams.size(),
			&rootParams[0],
			(UINT)staticSampler.size(),					// sampler register 숫자.
			&staticSampler[0],							// sampler register desc
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		);

		ComPtr<ID3DBlob> pSignature{}, pError{};
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
			return hr;
		hr = device->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
		if (FAILED(hr))
			return hr;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 4. pipe line 설정
	// Create the pipeline state once the shaders are loaded.
	{
		D3D12_RENDER_TARGET_BLEND_DESC alphaBlendDesc{};
		alphaBlendDesc.BlendEnable = true;
		alphaBlendDesc.LogicOpEnable = false;
		alphaBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		alphaBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		alphaBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		alphaBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		alphaBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		alphaBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		alphaBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
		alphaBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

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
		psoDesc.BlendState.RenderTarget[0] = alphaBlendDesc;
		hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
		if (FAILED(hr))
			return hr;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 5. 상수 + 텍스처 버퍼 heap 생성
	// Create a descriptor heap for the constant buffers.
	// ★★★★★★★★★★★★★★★
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = FRAME_BUFFER_COUNT * NUM_CBV + NUM_SRV;	// 프레임당 상수 버퍼 3개 (b0~b2) * 프레임 수 + 텍스처용 SRV(t0,t0) 2개
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvHeap));
		if (FAILED(hr))
			return hr;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 6. 상수 버퍼용 리소스 생성
	{
		UINT bufferSize = (UINT)G2::align256BufferSize(sizeof XMMATRIX);
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(FRAME_BUFFER_COUNT * bufferSize);

		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		hr = device->CreateCommittedResource(&uploadHeapProperties
												, D3D12_HEAP_FLAG_NONE
												, &constantBufferDesc
												, D3D12_RESOURCE_STATE_GENERIC_READ
												, nullptr, IID_PPV_ARGS(&m_cbv0rsc));
		if (FAILED(hr))
			return hr;
		CD3DX12_RANGE readRange(0, 0);
		hr = m_cbv0rsc->Map(0, &readRange, reinterpret_cast<void**>(&m_cbv0ptr));
		if (FAILED(hr))
			return hr;
	}
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
		// b0:
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_cbv0rsc->GetGPUVirtualAddress();
			UINT bufferSize = (UINT)G2::align256BufferSize(sizeof XMMATRIX);
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};

			for (int n = 0; n < FRAME_BUFFER_COUNT; ++n)
			{
				desc.BufferLocation = cbvGpuAddress;
				desc.SizeInBytes = bufferSize;
				device->CreateConstantBufferView(&desc, cbvCpuHandle);
				cbvGpuAddress += bufferSize;
				cbvCpuHandle.ptr += descriptorSize;
			}
		}
	}


	// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 7 텍스처 레지스터 SRV 디스크립터 생성
	// 실수 많이함. rootSigDesc 초기화 부분과 일치해야함.
	// ★★★★★★★★★★★★★★★
	// 3개 상수 레지스터 다음부터 텍스처 레지스터(셰이더 참고)
	//FRAME_BUFFER_COUNT * NUM_CBV
	{
		UINT baseSRVIndex = FRAME_BUFFER_COUNT * (UINT)NUM_CBV;
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(GetCpuHandle(m_cbvHeap.Get(), baseSRVIndex, descriptorSize));
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(GetGpuHandle(m_cbvHeap.Get(), baseSRVIndex, descriptorSize));

		// texture viewer 생성
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = m_textureRsc->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(m_textureRsc.Get(), &srvDesc, hCpuSrv);
			hCpuSrv.ptr += descriptorSize;
			hGpuSrv.ptr += descriptorSize;
		}
	}
	return S_OK;
}


int MainApp::SetupResource()
{
	HRESULT hr = S_OK;
	auto device    = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto cmdAlloc = std::any_cast<ID3D12CommandAllocator*>(IG2GraphicsD3D::getInstance()->GetCommandAllocator());
	auto cmdList  = std::any_cast<ID3D12GraphicsCommandList*>(IG2GraphicsD3D::getInstance()->GetCommandList());

	// 리소스 버텍스 버퍼 업로드
	// 버텍스 버퍼는 CreateCommittedResource 내부에서 heap 사용?
	Vertex cubeVertices[] =
	{
		{{-400.0f,  200.0f}, {255,   0,   0, 255}, {0.0f, 0.0f}},
		{{ 400.0f,  200.0f}, {  0, 255,   0, 255}, {1.0f, 0.0f}},
		{{ 400.0f, -200.0f}, {  0,   0, 255, 255}, {1.0f, 1.0f}},
		{{-400.0f, -200.0f}, {255,   0, 255, 255}, {0.0f, 1.0f}},
	};
	uint16_t indices[] = { 0, 1, 2, 0, 2, 3, };

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
		hr = device->CreateCommittedResource(&vtxHeapPropsGPU, D3D12_HEAP_FLAG_NONE, &vtxBufDesc
												, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_vtxGPU));
		if (FAILED(hr))
			return hr;

		// CPU 업로드 버퍼
		hr = device->CreateCommittedResource(&vtxHeapPropsUpload, D3D12_HEAP_FLAG_NONE, &vtxBufDesc
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
		hr = device->CreateCommittedResource(&idxHeapPropsGPU, D3D12_HEAP_FLAG_NONE, &idxBufDesc
												, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_idxGPU));
		if (FAILED(hr))
			return hr;

		// CPU 업로드 버퍼
		hr = device->CreateCommittedResource(&idxHeapPropsUpload, D3D12_HEAP_FLAG_NONE, &idxBufDesc
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

	hr = cmdAlloc->Reset();
	if(FAILED(hr))
		return hr;
	hr = cmdList->Reset(cmdAlloc, nullptr);
	if(FAILED(hr))
		return hr;

	// setup the vertex uploading
	{
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData      = reinterpret_cast<const BYTE*>(cubeVertices);
		vertexData.RowPitch   = m_vtxCount * sizeof(Vertex);
		vertexData.SlicePitch = m_vtxCount * sizeof(Vertex);
		UpdateSubresources(cmdList, m_vtxGPU.Get(), cpuUploaderVtx.Get(), 0, 0, 1, &vertexData);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_vtxGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		cmdList->ResourceBarrier(1, &barrier);
	}

	// setup the index bufer uploading
	{
		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData      = reinterpret_cast<const BYTE*>(indices);
		indexData.RowPitch   = m_idxCount * sizeof(uint16_t);
		indexData.SlicePitch = m_idxCount * sizeof(uint16_t);
		UpdateSubresources(cmdList, m_idxGPU.Get(), cpuUploaderIdx.Get(), 0, 0, 1, &indexData);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_idxGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		cmdList->ResourceBarrier(1, &barrier);
	}

	hr = cmdList->Close();
	if(FAILED(hr))
		return hr;

	// execute the uploading
	ID3D12CommandList* ppCommandLists[] = {cmdList};
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

