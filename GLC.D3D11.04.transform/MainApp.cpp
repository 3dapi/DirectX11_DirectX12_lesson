#include <any>
#include <Windows.h>
#include <d3d11.h>
#include <DirectxColors.h>
#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"
using namespace DirectX;

struct SimpleVertex
{
	DirectX::XMFLOAT3 p;	// position float
	uint8_t d[4];			// diffuse 32bit
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
	HRESULT hr = G2::CompileShaderFromFile("assets/simple.fxh", "main_vtx", "vs_4_0", &pBlob);
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
		{ "COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0 + sizeof(XMFLOAT3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = d3dDevice->CreateInputLayout(layout, numElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &m_vtxLayout);
	G2::SAFE_RELEASE(pBlob);
	if (FAILED(hr))
		return hr;


	// 2.1 Compile the pixel shader
	hr = G2::CompileShaderFromFile("assets/simple.fxh", "main_pxl", "ps_4_0", &pBlob);
	if (FAILED(hr))
	{
		MessageBox({}, "Failed ComplThe FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
		return hr;
	}
	// 2.2 Create the pixel shader
	hr = d3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), {}, &m_shaderPxl);
	if (FAILED(hr))
		return hr;

	// 3. Create vertex buffer
	SimpleVertex vertices[] =
    {
		{ {  -1.0F,  1.0F, -1.0F }, {    0,   0, 255, 255 } },
		{ {   1.0F,  1.0F, -1.0F }, {    0, 255,   0, 255 } },
		{ {   1.0F,  1.0F,  1.0F }, {    0, 255, 255, 255 } },
		{ {  -1.0F,  1.0F,  1.0F }, {  255,   0,   0, 255 } },
		{ {  -1.0F, -1.0F, -1.0F }, {  255,   0, 255, 255 } },
		{ {   1.0F, -1.0F, -1.0F }, {  255, 255,   0, 255 } },
		{ {   1.0F, -1.0F,  1.0F }, {  255, 255, 255, 255 } },
		{ {  -1.0F, -1.0F,  1.0F }, {   70,  70,  70, 255 } },
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
		0, 5, 4,  1, 5, 0,
		3, 4, 7,  0, 4, 3,
		1, 6, 5,  2, 6, 1,
		2, 7, 6,  3, 7, 2,
		6, 4, 5,  7, 4, 6,
	};
	m_bufIdxCount = sizeof(indices) / sizeof(indices[0]);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * m_bufIdxCount;        // 36 vertices needed for 12 triangles in a triangle list
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = d3dDevice->CreateBuffer(&bd, &InitData, &m_bufIdx);


	// 5. Create the constant buffer
	// world
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(m_mtWorld1);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstWorld);
	if (FAILED(hr))
		return hr;

	// view
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(m_mtView);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstView);
	if (FAILED(hr))
		return hr;

	// projection matrtix
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(m_mtProj);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = d3dDevice->CreateBuffer(&bd, {}, &m_cnstProj);
	if (FAILED(hr))
		return hr;


	// 6. setup the world, view, projection matrix
	// View, Projection Matrix
	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_mtView = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	auto screeSize = std::any_cast<::SIZE*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_CMD::ATTRIB_SCREEN_SIZE));
	m_mtProj = XMMatrixPerspectiveFovLH(XM_PIDIV2, screeSize->cx / (FLOAT)screeSize->cy, 0.01f, 100.0f);

	// 7. Initialize the world matrix
	m_mtWorld1 = XMMatrixIdentity();
	m_mtWorld2 = XMMatrixIdentity();

	return hr;
}

int MainApp::Destroy()
{
	G2::SAFE_RELEASE(m_shaderVtx);
	G2::SAFE_RELEASE(m_shaderPxl);
	G2::SAFE_RELEASE(m_vtxLayout);
	G2::SAFE_RELEASE(m_bufVtx	);
	G2::SAFE_RELEASE(m_bufIdx	);
	G2::SAFE_RELEASE(m_cnstWorld	);
	G2::SAFE_RELEASE(m_cnstView		);
	G2::SAFE_RELEASE(m_cnstProj		);
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
	// 1st cube world matrix
	m_mtWorld1 = XMMatrixRotationY(t);

	// 2nd Cube:  Rotate around origin
	XMMATRIX mSpin = XMMatrixRotationZ(-t);
	XMMATRIX mOrbit = XMMatrixRotationY(-t * 2.0f);
	XMMATRIX mTranslate = XMMatrixTranslation(-4.0f, 0.0f, 0.0f);
	XMMATRIX mScale = XMMatrixScaling(0.3f, 0.3f, 0.3f);
	m_mtWorld2 = mScale * mSpin * mTranslate * mOrbit;

	return S_OK;
}

int MainApp::Render()
{
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

	// 1. Update constant value
	d3dContext->UpdateSubresource(m_cnstWorld, 0, {}, &m_mtWorld1, 0, 0);
	d3dContext->UpdateSubresource(m_cnstView , 0, {}, &m_mtView , 0, 0);
	d3dContext->UpdateSubresource(m_cnstProj , 0, {}, &m_mtProj , 0, 0);

	// 2. set the constant buffer
	d3dContext->VSSetConstantBuffers(0, 1, &m_cnstWorld);
	d3dContext->VSSetConstantBuffers(1, 1, &m_cnstView);
	d3dContext->VSSetConstantBuffers(2, 1, &m_cnstProj);

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

	// 8. primitive topology
	d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 9.draw 1st cube
	d3dContext->DrawIndexed(m_bufIdxCount, 0, 0);

	// draw 2nd cube
	d3dContext->UpdateSubresource(m_cnstWorld, 0, {}, &m_mtWorld2, 0, 0);
	d3dContext->DrawIndexed(m_bufIdxCount, 0, 0);

	return S_OK;
}

