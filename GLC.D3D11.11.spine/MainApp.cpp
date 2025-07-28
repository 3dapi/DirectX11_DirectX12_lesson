#include <any>
#include <memory>
#include <vector>
#include <Windows.h>
#include <d3d11.h>
#include <DirectxColors.h>
#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"
using namespace DirectX;

// for lighting
struct SimpleVertex
{
	DirectX::XMFLOAT3 p;
	DirectX::XMFLOAT2 t;
};
// constant buffer for the vertex shader
struct ConstBufLight
{
	DirectX::XMFLOAT4 vLightDir[2];
	DirectX::XMFLOAT4 vLightColor[2];
	DirectX::XMFLOAT4 vOutputColor;
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
	auto d3dDevice  = std::any_cast<ID3D11Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

	// create vertex shader
	// 1.1 Compile the vertex shader
	ID3DBlob* pBlob{};
	HRESULT hr = G2::DXCompileShaderFromFile("assets/shader/simple.hlsl", "vs_4_0", "main_vtx", &pBlob);
	if (FAILED(hr))
	{
		MessageBox({}, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
		return hr;
	}
	// 1.2 Create the vertex shader
	hr = d3dDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), {}, &m_shaderVtx);
	if (FAILED(hr))
		return hr;

	// 1.3 create vertexLayout
	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT   , 0, 0 + sizeof(DirectX::XMFLOAT3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = d3dDevice->CreateInputLayout(layout, numElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &m_vtxLayout);
	G2::SAFE_RELEASE(pBlob);
	if (FAILED(hr))
		return hr;


	// 2.1 Compile the pixel shader
	hr = G2::DXCompileShaderFromFile("assets/shader/simple.hlsl", "ps_4_0", "main_pxl", &pBlob);
	if (FAILED(hr))
	{
		MessageBox({}, "Failed ComplThe FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
		return hr;
	}
	// 2.2 Create the pixel shader
	hr = d3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), {}, &m_shaderPxl);
	G2::SAFE_RELEASE(pBlob);
	if (FAILED(hr))
		return hr;

	// 3. Create vertex buffer
	SimpleVertex vertices[] =
	{
		{ { -1.0f,  1.0f, -1.0f },  { 1.0f, 0.0f } },
		{ {  1.0f,  1.0f, -1.0f },  { 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  1.0f },  { 0.0f, 1.0f } },
		{ { -1.0f,  1.0f,  1.0f },  { 1.0f, 1.0f } },
   
		{ { -1.0f, -1.0f, -1.0f },  { 0.0f, 0.0f } },
		{ {  1.0f, -1.0f, -1.0f },  { 1.0f, 0.0f } },
		{ {  1.0f, -1.0f,  1.0f },  { 1.0f, 1.0f } },
		{ { -1.0f, -1.0f,  1.0f },  { 0.0f, 1.0f } },
   
		{ { -1.0f, -1.0f,  1.0f },  { 0.0f, 1.0f } },
		{ { -1.0f, -1.0f, -1.0f },  { 1.0f, 1.0f } },
		{ { -1.0f,  1.0f, -1.0f },  { 1.0f, 0.0f } },
		{ { -1.0f,  1.0f,  1.0f },  { 0.0f, 0.0f } },
   
		{ {  1.0f, -1.0f,  1.0f },  { 1.0f, 1.0f } },
		{ {  1.0f, -1.0f, -1.0f },  { 0.0f, 1.0f } },
		{ {  1.0f,  1.0f, -1.0f },  { 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  1.0f },  { 1.0f, 0.0f } },
   
		{ { -1.0f, -1.0f, -1.0f },  { 0.0f, 1.0f } },
		{ {  1.0f, -1.0f, -1.0f },  { 1.0f, 1.0f } },
		{ {  1.0f,  1.0f, -1.0f },  { 1.0f, 0.0f } },
		{ { -1.0f,  1.0f, -1.0f },  { 0.0f, 0.0f } },
   
		{ { -1.0f, -1.0f,  1.0f },  { 1.0f, 1.0f } },
		{ {  1.0f, -1.0f,  1.0f },  { 0.0f, 1.0f } },
		{ {  1.0f,  1.0f,  1.0f },  { 0.0f, 0.0f } },
		{ { -1.0f,  1.0f,  1.0f },  { 1.0f, 0.0f } },
    };
	m_bufVtxCount = sizeof(vertices) / sizeof(vertices[0]);
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * m_bufVtxCount;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;
	hr = d3dDevice->CreateBuffer(&bd, &InitData, &m_bufVtx);
	if (FAILED(hr))
		return hr;

	// 4. Create Index buffer
	WORD indices[] =
	{
		 3, 1, 0,  2, 1, 3,
		 6, 4, 5,  7, 4, 6,
		11, 9, 8, 10, 9,11,
		14,12,13, 15,12,14,
		19,17,16, 18,17,19,
		22,20,21, 23,20,22
	};
	m_bufIdxCount = sizeof(indices) / sizeof(indices[0]);

	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(WORD) * m_bufIdxCount;        // 36 vertices needed for 12 triangles in a triangle list
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		InitData.pSysMem = indices;
		hr = d3dDevice->CreateBuffer(&bd, &InitData, &m_bufIdx);
	}

	// 5. Create the constant buffer
	// world
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(m_mtWorld);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstWorld);
		if (FAILED(hr))
			return hr;
	}

	// view
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(m_mtView);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstView);
		if (FAILED(hr))
			return hr;
	}

	// projection matrtix
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(m_mtProj);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstProj);
		if (FAILED(hr))
			return hr;
	}

	// Diffuse color for the mesh
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(m_difMesh);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstDiff);
		if(FAILED(hr))
			return hr;
	}

	// 6. setup the world, view, projection matrix
	// View, Projection Matrix
	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet( 0.0f, 4.0f, -10.0f, 0.0f );
	XMVECTOR At = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	XMVECTOR Up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	m_mtView = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	auto screeSize = std::any_cast<::SIZE*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_CMD::ATTRIB_SCREEN_SIZE));
	m_mtProj = XMMatrixPerspectiveFovLH(XM_PIDIV4, screeSize->cx / (FLOAT)screeSize->cy, 1.0f, 5000.0f);

	// 7. Initialize the world matrix
	m_mtWorld = XMMatrixIdentity();

	// 8. create texture sampler state
	// Load the Texture from string
	hr  = InitTexture();

	D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = d3dDevice->CreateBlendState(&blendDesc, &m_blendAlpha);
	if(FAILED(hr))
		return hr;

	return S_OK;
}

int MainApp::Destroy()
{
	G2::SAFE_RELEASE(m_shaderVtx);
	G2::SAFE_RELEASE(m_shaderPxl);
	G2::SAFE_RELEASE(m_vtxLayout);
	G2::SAFE_RELEASE(m_bufVtx	);
	G2::SAFE_RELEASE(m_bufIdx	);
	G2::SAFE_RELEASE(m_cnstWorld);
	G2::SAFE_RELEASE(m_cnstView	);
	G2::SAFE_RELEASE(m_cnstProj	);

	G2::SAFE_RELEASE(m_srvTexture);
	G2::SAFE_RELEASE(m_sampLinear);
	G2::SAFE_RELEASE(m_cnstDiff	);
	G2::SAFE_RELEASE(m_blendAlpha );
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

	// Rotate cube around the origin
	m_mtWorld = XMMatrixRotationY(t);

	m_difMesh.x = (sinf(t * 1.0f) + 1.0f) * 0.5f;
	m_difMesh.y = (cosf(t * 3.0f) + 1.0f) * 0.5f;
	m_difMesh.z = (sinf(t * 5.0f) + 1.0f) * 0.5f;

	return S_OK;
}

int MainApp::Render()
{
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

	// 1. Update constant value
	d3dContext->UpdateSubresource(m_cnstWorld, 0, {}, &m_mtWorld, 0, 0);
	d3dContext->UpdateSubresource(m_cnstView , 0, {}, &m_mtView , 0, 0);
	d3dContext->UpdateSubresource(m_cnstProj , 0, {}, &m_mtProj , 0, 0);
	d3dContext->UpdateSubresource(m_cnstDiff , 0, {}, &m_difMesh , 0, 0);

	// 2. set the constant buffer
	d3dContext->VSSetConstantBuffers(0, 1, &m_cnstWorld);
	d3dContext->VSSetConstantBuffers(1, 1, &m_cnstView);
	d3dContext->VSSetConstantBuffers(2, 1, &m_cnstProj);
	d3dContext->PSSetConstantBuffers(3, 1, &m_cnstDiff);

	float blendFactor[4] = {0, 0, 0, 0}; // optional
	UINT sampleMask = 0xffffffff;
	d3dContext->OMSetBlendState(m_blendAlpha, blendFactor, sampleMask);

	// 3. set vertex shader
	d3dContext->VSSetShader(m_shaderVtx, {}, 0);

	// 4. set the input layout
	d3dContext->IASetInputLayout(m_vtxLayout);

	// 5. set the pixel shader
	d3dContext->PSSetShader(m_shaderPxl, {}, 0);

	// 6. set the vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	d3dContext->IASetVertexBuffers(0, 1, &m_bufVtx, &stride, &offset);

	// 7. set the index buffer
	d3dContext->IASetIndexBuffer(m_bufIdx, DXGI_FORMAT_R16_UINT, 0);

	// 8. set the texture
	d3dContext->PSSetShaderResources(0, 1, &m_srvTexture);

	// 9. set the sampler state
	d3dContext->PSSetSamplers(0, 1, &m_sampLinear);

	// 10. primitive topology
	d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 11.draw cube
	d3dContext->DrawIndexed(m_bufIdxCount, 0, 0);

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
	auto d3dDevice  = std::any_cast<ID3D11Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

	std::string testMessage = "다람쥐 헌 쳇바퀴에 타고파";

	hr = AddFontResourceEx("assets/font/GodoB.ttf", FR_PRIVATE, nullptr);

	HFONT hFont = CreateFont(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
							  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
							  DEFAULT_PITCH | FF_DONTCARE, "고도 B");

	auto strSize = StringSize(testMessage, hFont);

	// 4의 배수로 맞춤
	const int width = (strSize.cx +4)/4 * 4;
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
	RemoveFontResourceEx("assets/font/GodoB.ttf", FR_PRIVATE, nullptr);

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

	// 1. Create ID3D11Texture2D
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width              = width;
	texDesc.Height             = height;
	texDesc.MipLevels          = 1;
	texDesc.ArraySize          = 1;
	texDesc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count   = 1;
	texDesc.Usage              = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags     = 0;
	texDesc.MiscFlags          = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = pixels;
	initData.SysMemPitch = width * 4;

	ID3D11Texture2D* tex = nullptr;
	hr = d3dDevice->CreateTexture2D(&texDesc, &initData, &tex);
	if (FAILED(hr))
		return hr;

	// 2. Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = d3dDevice->CreateShaderResourceView(tex, &srvDesc, &m_srvTexture);
	tex->Release();
	if (FAILED(hr))
		return hr;

	// 3. Create sampler
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD         = 0;
	sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;

	hr = d3dDevice->CreateSamplerState(&sampDesc, &m_sampLinear);
	return hr;
}
