// Implementation of the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#include <directxcolors.h>
#include "D3DApp.h"

D3DApp* D3DApp::m_pAppMain{};
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
		D3DApp::m_pAppMain={};
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
	float clearColor[4] = { 0.0f, 0.4f, 0.6f, 1.0f };
	m_d3dContext->ClearRenderTargetView(m_d3dRenderTargetView.Get(), clearColor);
	m_d3dContext->ClearDepthStencilView(m_d3dDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0F, 0 );
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
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = sizeof(featureLevels)/sizeof(featureLevels[0]);
	// Create the D3D11.1 device and context
	hr = D3D11CreateDevice(nullptr, driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION
							, m_d3dDevice.GetAddressOf(), &m_featureLevel, m_d3dContext.GetAddressOf());
	if (hr == E_INVALIDARG)
	{
		// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
		hr = D3D11CreateDevice(nullptr, driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1, D3D11_SDK_VERSION
							,  m_d3dDevice.GetAddressOf(), &m_featureLevel, m_d3dContext.GetAddressOf());
	}
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	ComPtr<IDXGIFactory1> dxgiFactory{};
	{
		ComPtr<IDXGIDevice> dxgiDevice{};
		hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.GetAddressOf()));
		if (SUCCEEDED(hr))
		{
			ComPtr<IDXGIAdapter> adapter{};
			hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory.GetAddressOf()));
			}
		}
	}
	if (FAILED(hr))
		return hr;

	// Create swap chain
	ComPtr<IDXGIFactory2> dxgiFactory2{};
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(dxgiFactory2.GetAddressOf()));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = m_d3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(m_d3dDevice_1.GetAddressOf()));
		if (SUCCEEDED(hr))
		{
			m_d3dContext->QueryInterface( __uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(m_d3dContext_1.GetAddressOf()) );
		}

		DXGI_SWAP_CHAIN_DESC1 descSwap{};
		descSwap.Width = m_screenSize.cx;
		descSwap.Height = m_screenSize.cy;
		descSwap.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		descSwap.SampleDesc.Count = 1;
		descSwap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		descSwap.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(m_d3dDevice.Get(), m_hWnd, &descSwap, nullptr, nullptr, m_d3dSwapChain_1.GetAddressOf());
		if (SUCCEEDED(hr))
		{
			hr = m_d3dSwapChain_1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(m_d3dSwapChain.GetAddressOf()));
		}
		dxgiFactory2.Reset();
	}
	else
	{
		DXGI_SWAP_CHAIN_DESC descSwap{};
		descSwap.BufferCount = 1;
		descSwap.BufferDesc.Width = m_screenSize.cx;
		descSwap.BufferDesc.Height = m_screenSize.cy;
		descSwap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		descSwap.BufferDesc.RefreshRate.Numerator = 60;
		descSwap.BufferDesc.RefreshRate.Denominator = 1;
		descSwap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		descSwap.OutputWindow = m_hWnd;
		descSwap.SampleDesc.Count = 1;
		descSwap.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(m_d3dDevice.Get(), &descSwap, m_d3dSwapChain.GetAddressOf());
	}
	if (FAILED(hr))
		return hr;

	// block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
	dxgiFactory.Reset();

	// Create a render target view
	ComPtr<ID3D11Texture2D> pBackBuffer{};
	hr = m_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));
	if (FAILED(hr))
		return hr;
	hr = m_d3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, m_d3dRenderTargetView.GetAddressOf());
	pBackBuffer.Reset();
	if (FAILED(hr))
		return hr;
	m_d3dContext->OMSetRenderTargets(1, m_d3dRenderTargetView.GetAddressOf(), nullptr);


	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth{};
	descDepth.Width = m_screenSize.cx;
	descDepth.Height = m_screenSize.cy;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	hr = m_d3dDevice->CreateTexture2D(&descDepth, nullptr, m_d3dDepthStencil.GetAddressOf());
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV{};
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = m_d3dDevice->CreateDepthStencilView(m_d3dDepthStencil.Get(), &descDSV, m_d3dDepthStencilView.GetAddressOf());
	if (FAILED(hr))
		return hr;

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
	m_d3dDevice				.Reset();
	m_d3dContext			.Reset();
	m_d3dSwapChain			.Reset();
	m_d3dDevice_1			.Reset();
	m_d3dContext_1			.Reset();
	m_d3dSwapChain_1		.Reset();
	m_d3dRenderTargetView	.Reset();
	m_d3dDepthStencil		.Reset();
	m_d3dDepthStencilView	.Reset();

	return 0;
}
