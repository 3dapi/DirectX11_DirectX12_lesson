// Implementation of the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#include <directxcolors.h>
#include "D3DApp.h"
#include "G2Util.h"
#include "MainApp.h"

D3DApp* D3DApp::m_pAppMain{};
D3DApp::D3DApp()
{
	m_name = "DirectX 12 App";
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

int D3DApp::Create(void* initialist)
{
	if (initialist)
	{
		m_hInst = (HINSTANCE)initialist;
	}
	else
	{
		m_hInst = (HINSTANCE)GetModuleHandle(nullptr);
	}

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

	if (FAILED(InitDevice()))	// Initialize the D3D device
	{
		goto END;
	}
	if (FAILED(Init()))			// Initialize the main game
	{
		goto END;
	}
	return S_OK;
END:
	Cleanup();
	return E_FAIL;
}

std::any D3DApp::GetAttrib(int nCmd)
{
	switch (nCmd)
	{
		case ATTRIB_CMD::ATTRIB_DEVICE:			return m_d3dDevice;
		case ATTRIB_CMD::ATTRIB_CONTEXT:		return nullptr;
		case ATTRIB_CMD::ATTRIB_SCREEN_SIZE:	return &m_screenSize;
	}
	return nullptr;
}

std::any D3DApp::GetDevice()
{
	return m_d3dDevice;
}

std::any D3DApp::GetContext()
{
	return nullptr;
}

void D3DApp::Cleanup()
{
	Destroy();
	ReleaseDevice();
	::DestroyWindow(m_hWnd);
	::UnregisterClass(m_class.c_str(), m_hInst);
	if (D3DApp::m_pAppMain)
	{
		G2::SAFE_DELETE(D3DApp::m_pAppMain);
	}
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
			RenderApp();
		}
	}
	Cleanup();
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

		case WM_SYSKEYDOWN:
			if (wParam == VK_RETURN)
			{
				ToggleFullScreen();
				return 0;
			}
			break;

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
			::PostQuitMessage(0);
			return 0;
		}
	}

	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void D3DApp::ToggleFullScreen()
{
	m_bFullScreen ^= 1;
	//m_d3dSwapChain->SetFullscreenState(m_bFullScreen, {} );
}

int D3DApp::RenderApp()
{
	// update rendering data
	Update();

	// rendering
	Render();

	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_d3dCommandList.Get() };
	m_d3dCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	HRESULT hr = m_d3dSwapChain->Present(1, 0);

	WaitForPreviousFrame();
	return hr;
}


int D3DApp::InitDevice()
{
	HRESULT hr = S_OK;
	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif
	ComPtr<IDXGIFactory4> factory;
	hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

	IDXGIAdapter* pAdapter = nullptr;
	if (m_useHardware)
	{
		pAdapter = GetHardwareAdapter(factory.Get());
	}
	else
	{
		factory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter));
	}

	// 1. create device with best feature
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	for (int i = 0; i < sizeof(featureLevels) / sizeof(featureLevels[0]); ++i)
	{
		hr = D3D12CreateDevice(pAdapter, featureLevels[i], IID_PPV_ARGS(&m_d3dDevice));
		if (SUCCEEDED(hr))
		{
			m_featureLevel = featureLevels[i];
			break;
		}
	}
	G2::SAFE_RELEASE(pAdapter);
	if (!m_featureLevel)
		return E_FAIL;


	// 2. create command queue
	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_d3dCommandQueue));
	if (FAILED(hr))
		return hr;

	// 3 create swapchain
	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 descSwap = {};
	descSwap.BufferCount = FRAME_COUNT;
	descSwap.Width       = m_screenSize.cx;
	descSwap.Height      = m_screenSize.cy;
	descSwap.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
	descSwap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	descSwap.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	descSwap.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	hr = factory->CreateSwapChainForHwnd(
		m_d3dCommandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		GetHwnd(),
		&descSwap,
		nullptr,
		nullptr,
		&swapChain);
	if (FAILED(hr))
		return hr;

	// not support fullscreen transitions.
	hr = factory->MakeWindowAssociation(GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(hr))
		return hr;

	// setup d3dSwapchain
	hr = swapChain.As(&m_d3dSwapChain);
	if (FAILED(hr))
		return hr;

	// 4. setup frame Index
	m_d3dFrameIndex = m_d3dSwapChain->GetCurrentBackBufferIndex();

	// 5. Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FRAME_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_d3dDescHeap));
		if (FAILED(hr))
			return hr;
		m_d3dDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// 6. Create frame resources.
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_d3dDescHeap->GetCPUDescriptorHandleForHeapStart();
		// Create a RTV for each frame.
		for (UINT n = 0; n < FRAME_COUNT; n++)
		{
			hr = m_d3dSwapChain->GetBuffer(n, IID_PPV_ARGS(&m_d3dRenderTarget[n]));
			if (FAILED(hr))
				return hr;
			m_d3dDevice->CreateRenderTargetView(m_d3dRenderTarget[n].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += m_d3dDescriptorSize;
		}
	}
	hr = m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_d3dCommandAllocator));
	if (FAILED(hr))
		return hr;

	// Create the command list.
	hr = m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_d3dCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_d3dCommandList));
	if (FAILED(hr))
		return hr;

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	hr = m_d3dCommandList->Close();
	if (FAILED(hr))
		return hr;

	// Create synchronization objects.
	{
		hr = m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
		if (FAILED(hr))
			return hr;
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			if (FAILED(hr))
				return hr;
		}
	}

	return S_OK;
}

int D3DApp::ReleaseDevice()
{
	Destroy();

	return S_OK;
}

IDXGIAdapter* D3DApp::GetHardwareAdapter(IDXGIFactory1* pFactory, bool requestHighPerformanceAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<IDXGIFactory6> factory6;
	if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
	{
		for (
			UINT adapterIndex = 0;
			SUCCEEDED(factory6->EnumAdapterByGpuPreference(
				adapterIndex,
				requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
				IID_PPV_ARGS(&adapter)));
				++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}
			// Check to see whether the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}
	if (adapter.Get() == nullptr)
	{
		for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see whether the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}
	IDXGIAdapter1* pAdapter = adapter.Detach();
	return pAdapter;
}

HRESULT D3DApp::PopulateCommandList()
{
	HRESULT hr = S_OK;

	// Command allocator는 GPU가 해당 작업을 마친 후에만 Reset 가능.
	hr = m_d3dCommandAllocator->Reset();
	if (FAILED(hr))
		return hr;

	// Command list는 Execute 후 언제든 Reset 가능. 반드시 Reset 후 재사용해야 함.
	hr = m_d3dCommandList->Reset(m_d3dCommandAllocator.Get(), m_d3dPipelineState.Get());
	if (FAILED(hr))
		return hr;

	// --- 1. 리소스 상태 전환: Present → RenderTarget ---
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_d3dRenderTarget[m_d3dFrameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_d3dCommandList->ResourceBarrier(1, &barrier);

	// --- 2. 렌더 타겟 핸들 설정 ---
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_d3dDescHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += static_cast<SIZE_T>(m_d3dFrameIndex) * static_cast<SIZE_T>(m_d3dDescriptorSize);

	// --- 3. 렌더 타겟 클리어 ---
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_d3dCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// --- 4. 리소스 상태 전환: RenderTarget → Present ---
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	m_d3dCommandList->ResourceBarrier(1, &barrier);

	// --- 5. 명령 리스트 종료 ---
	hr = m_d3dCommandList->Close();

	return hr;
}

HRESULT D3DApp::WaitForPreviousFrame()
{
	HRESULT hr = S_OK;
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	hr = m_d3dCommandQueue->Signal(m_fence.Get(), fence);
	if (FAILED(hr))
		return hr;

	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		hr = m_fence->SetEventOnCompletion(fence, m_fenceEvent);
		if (FAILED(hr))
			return hr;
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_d3dFrameIndex = m_d3dSwapChain->GetCurrentBackBufferIndex();
	return S_OK;
}

int D3DApp::Init()
{
	m_pmain = new class MainApp;
	if (!m_pmain)
		return E_FAIL;
	if (FAILED(m_pmain->Init()))
	{
		G2::SAFE_DELETE(m_pmain);
		return E_FAIL;
	}
	return S_OK;
}
int D3DApp::Destroy()
{
	G2::SAFE_DELETE(m_pmain);
	return S_OK;
}
int D3DApp::Update()
{
	if (m_pmain)
		m_pmain->Update();

	return S_OK;
}
int D3DApp::Render()
{
	if(m_pmain)
		m_pmain->Render();
	return S_OK;
}