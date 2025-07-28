#include <any>
#include <Windows.h>
#include <d3d11.h>
#include <DirectxMath.h>
#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"

struct SimpleVertex
{
	DirectX::XMFLOAT3 Pos;
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
	// 1. 1Compile the vertex shader
	ID3DBlob* pBlob{};
	HRESULT hr = G2::CompileShaderFromFile("assets/simple.fxh", "main_vtx", "vs_4_0", &pBlob);
	if (FAILED(hr))
	{
		MessageBox({}, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
		return hr;
	}
	// 1.2 Create the vertex shader
	hr = d3dDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), {}, &m_shaderVtx);
	// 1.2 create vertexLayout
	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);
	// 1.3 Create the input layout
	hr = d3dDevice->CreateInputLayout(layout, numElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &m_vtxLayout);
	G2::SAFE_RELEASE(pBlob);
	if (FAILED(hr))
		return hr;

	// 2.1 Compile the pixel shader
	pBlob = nullptr;
	hr = G2::CompileShaderFromFile("assets/simple.fxh", "main_pxl", "ps_4_0", &pBlob);
	if (FAILED(hr))
	{
		MessageBox({}, "Failed ComplThe FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
		return hr;
	}
	// 2.2 Create the pixel shader
	hr = d3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_shaderPxl);

	// 3. Create vertex buffer
	SimpleVertex vertices[] =
	{
		DirectX::XMFLOAT3(-0.5F,  0.51F, 0.5F),
		DirectX::XMFLOAT3( 0.5F,  0.51F, 0.5F),
		DirectX::XMFLOAT3( 0.5F, -0.51F, 0.5F),

		DirectX::XMFLOAT3(-0.5F,  0.49F, 0.5F),
		DirectX::XMFLOAT3( 0.5F, -0.53F, 0.5F),
		DirectX::XMFLOAT3(-0.5F, -0.53F, 0.5F),
	};
	m_vtxCount = sizeof(vertices) / sizeof(vertices[0]);
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * m_vtxCount;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;
	hr = d3dDevice->CreateBuffer(&bd, &InitData, &m_vtxBuffer);
	return hr;
}

int MainApp::Destroy()
{
	G2::SAFE_RELEASE(m_shaderVtx);
	G2::SAFE_RELEASE(m_shaderPxl);
	G2::SAFE_RELEASE(m_vtxLayout);
	G2::SAFE_RELEASE(m_vtxBuffer);
	return S_OK;
}

int MainApp::Update()
{
	return S_OK;
}

int MainApp::Render()
{
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

	// Render a triangle
	d3dContext->VSSetShader(m_shaderVtx, nullptr, 0);
	d3dContext->PSSetShader(m_shaderPxl, nullptr, 0);
	// Set the input layout
	d3dContext->IASetInputLayout(m_vtxLayout);

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	d3dContext->IASetVertexBuffers(0, 1, &m_vtxBuffer, &stride, &offset);

	// Set primitive topology
	d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3dContext->Draw(m_vtxCount, 0);
	return S_OK;
}

