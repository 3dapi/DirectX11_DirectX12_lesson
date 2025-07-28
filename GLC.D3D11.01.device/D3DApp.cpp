// Implementation of the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#include <directxcolors.h>
#include <wrl/client.h>
#include "D3DApp.h"

using Microsoft::WRL::ComPtr;

D3DApp* D3DApp::m_pAppMain = {};
D3DApp::D3DApp()
{
	m_name = "DirectX 11 App";
}

LRESULT WINAPI D3DApp::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return m_pAppMain->MsgProc(hWnd, msg, wParam, lParam);
}

D3DApp* D3DApp::getInstance()
{
	if (!D3DApp::m_pAppMain)
	{
		D3DApp::m_pAppMain = new D3DApp;
	}
	return D3DApp::m_pAppMain;
}

int D3DApp::Create(HINSTANCE hInst)
{
	m_hInst = hInst;

	WNDCLASS wc =								// Register the window class
	{
		CS_CLASSDC
		, D3DApp::WndProc
		, 0L
		, 0L
		, m_hInst
		, nullptr
		, ::LoadCursor(nullptr,IDC_ARROW)
		, (HBRUSH)::GetStockObject(WHITE_BRUSH)
		, nullptr
		, m_class.c_str()
	};

	if (!::RegisterClass(&wc))
		return E_FAIL;

	RECT rc{};									//Create the application's window
	::SetRect(&rc, 0, 0, m_screenSize.cx, m_screenSize.cy);
	::AdjustWindowRect(&rc, m_dWinStyle, FALSE);

	int iScnSysW = ::GetSystemMetrics(SM_CXSCREEN);
	int iScnSysH = ::GetSystemMetrics(SM_CYSCREEN);

	// window 생성.
	m_hWnd = ::CreateWindow(
						  m_class.c_str()
						, m_name.c_str()
						, m_dWinStyle
						, (iScnSysW - (rc.right - rc.left)) / 2
						, (iScnSysH - (rc.bottom - rc.top)) / 2
						, (rc.right - rc.left)
						, (rc.bottom - rc.top)
						, nullptr
						, nullptr
						, m_hInst
						, nullptr
	);
	::ShowWindow(m_hWnd, SW_SHOW);
	::UpdateWindow(m_hWnd);
	::ShowCursor(m_showCusor);

	auto ret = InitDevice();
	return ret;
}


void D3DApp::Cleanup()
{
}


int D3DApp::Run()
{
	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}
	::UnregisterClass(m_class.c_str(), m_hInst);
	if(D3DApp::m_pAppMain)
	{
		delete D3DApp::m_pAppMain;
		D3DApp::m_pAppMain = {};
	}
	return ERROR_SUCCESS;
}


LRESULT D3DApp::MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_PAINT:
		{
			break;
		}

		case WM_KEYDOWN:
		{

			switch (wParam)
			{
			case VK_ESCAPE:
			{
				::SendMessage(hWnd, WM_DESTROY, 0, 0);
				break;
			}
			}

			return 0;

		}

		case WM_DESTROY:
		{
			Cleanup();
			::PostQuitMessage(0);
			return 0;
		}
	}

	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

int D3DApp::Render()
{
	m_d3dContext->ClearRenderTargetView(m_d3dRenderTargetView, DirectX::Colors::MidnightBlue);
	m_d3dSwapChain->Present(0, 0);
	return ERROR_SUCCESS;
}


int D3DApp::InitDevice()
{
	HRESULT hr = S_OK;
	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	hr = D3D11CreateDevice(nullptr, driverType, nullptr, createDeviceFlags, &featureLevel, 1, D3D11_SDK_VERSION, &m_d3dDevice, &m_featureLevel, &m_d3dContext);
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = m_screenSize.cx;
	sd.BufferDesc.Height = m_screenSize.cy;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = m_hWnd;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;

	hr = dxgiFactory->CreateSwapChain(m_d3dDevice, &sd, &m_d3dSwapChain);
	if (FAILED(hr))
		return hr;
	// block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
	dxgiFactory->Release();	

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = m_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;
	hr = m_d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_d3dRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;
	m_d3dContext->OMSetRenderTargets(1, &m_d3dRenderTargetView, nullptr);

	// Setup the viewport
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)m_screenSize.cx;
	vp.Height = (FLOAT)m_screenSize.cy;
	vp.MaxDepth = 1.0f;
	m_d3dContext->RSSetViewports(1, &vp);
	return hr;
}

int D3DApp::ReleaseDevice()
{
	if (m_d3dDevice)	m_d3dDevice->Release();
	if (m_d3dContext)	m_d3dContext->Release();
	if (m_d3dSwapChain)	m_d3dSwapChain->Release();
	if (m_d3dRenderTargetView)	m_d3dRenderTargetView->Release();

	return 0;
}
