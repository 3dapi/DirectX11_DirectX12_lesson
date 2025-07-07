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
		case ATTRIB_CMD::ATTRIB_DEVICE_RENDER_TARGET_FORAT:	return &m_formatBackBuffer;
		case ATTRIB_CMD::ATTRIB_DEVICE_DEPTH_STENCIL_FORAT:	return &m_formatDepthBuffer;
		case ATTRIB_CMD::ATTRIB_DEVICE_CURRENT_FRAME_INDEX:
		{
			m_d3dCurrentFrameIndex = m_d3dSwapChain->GetCurrentBackBufferIndex();
			return &m_d3dCurrentFrameIndex;
		}

		case ATTRIB_CMD::ATTRIB_DEVICE_VIEWPORT:		return &m_d3dViewport;
		case ATTRIB_CMD::ATTRIB_DEVICE_SCISSOR_RECT:	return &m_d3dScissorRect;
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

std::any D3DApp::GetRootSignature()
{
	return m_d3dRootSignature.Get();
}

std::any D3DApp::GetCommandAllocator()
{
	return m_d3dCommandAllocator[m_d3dCurrentFrameIndex].Get();
}

std::any D3DApp::GetCommandQueue()
{
	return m_d3dCommandQueue.Get();
}

std::any D3DApp::GetCommandList()
{
	return m_d3dCommandList.Get();
}

std::any D3DApp::GetRenderTarget()
{
	return m_d3dRenderTarget[m_d3dCurrentFrameIndex].Get();
}

std::any D3DApp::GetRenderTargetView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_d3dDescTarget->GetCPUDescriptorHandleForHeapStart(), m_d3dCurrentFrameIndex, m_d3dSizeDescRenderTarget);
}
std::any D3DApp::GetDepthStencilView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_d3dDescDepth->GetCPUDescriptorHandleForHeapStart());
}

int D3DApp::GetCurrentFrameIndex() const
{
	return m_d3dCurrentFrameIndex;
}

void D3DApp::Cleanup()
{
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
	Update();


	HRESULT hr = S_OK;
	auto d3dDevice         = std::any_cast<ID3D12Device*            >(IG2GraphicsD3D::getInstance()->GetDevice());
	auto commandQue        = std::any_cast<ID3D12CommandQueue*      >(IG2GraphicsD3D::getInstance()->GetCommandQueue());
	auto commandAlloc      = std::any_cast<ID3D12CommandAllocator*  >(IG2GraphicsD3D::getInstance()->GetCommandAllocator());
	auto currentFrameIndex = std::any_cast<int                      >(IG2GraphicsD3D::getInstance()->GetCurrentFrameIndex());
	auto renderTarget      = std::any_cast<ID3D12Resource*          >(IG2GraphicsD3D::getInstance()->GetRenderTarget());

	CD3DX12_RESOURCE_BARRIER barrier_begin = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CD3DX12_RESOURCE_BARRIER barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);


    hr = commandAlloc->Reset();
    hr = m_d3dCommandList->Reset(commandAlloc, nullptr);  // PSO는 루프 내에서 설정 예정

    // 뷰포트, 시저, 렌더타겟 뷰/DSV
    D3D12_VIEWPORT* viewport = std::any_cast<D3D12_VIEWPORT*>(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_VIEWPORT));
    D3D12_RECT*     rcScissor= std::any_cast<D3D12_RECT*    >(IG2GraphicsD3D::getInstance()->GetAttrib(ATTRIB_DEVICE_SCISSOR_RECT));
    auto renderTargetView    = std::any_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(IG2GraphicsD3D::getInstance()->GetRenderTargetView());
    auto depthStencilView    = std::any_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(IG2GraphicsD3D::getInstance()->GetDepthStencilView());

    // 상태 전환: Present → RenderTarget
    m_d3dCommandList->ResourceBarrier(1, &barrier_begin);

    // Clear
    float clearColor[] = { 0.0f, 0.4f, 0.6f, 1.0f };
    m_d3dCommandList->ClearRenderTargetView(renderTargetView, clearColor, 0, nullptr);
    m_d3dCommandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 타겟 설정, 뷰포트 설정
    m_d3dCommandList->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);
    m_d3dCommandList->RSSetViewports(1, viewport);
    m_d3dCommandList->RSSetScissorRects(1, rcScissor);

	Render();

    // 상태 전환: RenderTarget → Present
    m_d3dCommandList->ResourceBarrier(1, &barrier_end);

    hr = m_d3dCommandList->Close();

    ID3D12CommandList* ppCommandLists[] = { m_d3dCommandList.Get() };
    commandQue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


	hr = m_d3dSwapChain->Present(0, 0);

	WaitForGpu();
	MoveToNextFrame();
	return hr;
}


int D3DApp::InitDevice()
{
	HRESULT hr = S_OK;
	m_d3dViewport    = { 0.0F, 0.0F, (float)(m_screenSize.cx), (float)(m_screenSize.cy)};
	m_d3dScissorRect = { 0, 0, (LONG)(m_screenSize.cx), (LONG)(m_screenSize.cy) };

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

	// 1.1 Create an empty root signature.
	{
		D3D12_ROOT_SIGNATURE_DESC rsDesc{};
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (FAILED(hr))
			return hr;
		hr = m_d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_d3dRootSignature));
		if (FAILED(hr))
			return hr;
	}

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
		descSwap.BufferCount = FRAME_BUFFER_COUNT;
		descSwap.Width       = m_screenSize.cx;
		descSwap.Height      = m_screenSize.cy;
		descSwap.Format      = m_formatBackBuffer;
		descSwap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		descSwap.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		descSwap.SampleDesc.Count = 1;

	hr = factory->CreateSwapChainForHwnd(
		m_d3dCommandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		GetHwnd(),
		&descSwap,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)m_d3dSwapChain.GetAddressOf()
	);
	if (FAILED(hr))
		return hr;

	// not support fullscreen transitions.
	hr = factory->MakeWindowAssociation(GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(hr))
		return hr;

	// 4. setup frame Index
	m_d3dCurrentFrameIndex = m_d3dSwapChain->GetCurrentBackBufferIndex();

	// 5. Create render target and depth-stencil descriptor heaps.
	{
		m_d3dSizeDescRenderTarget = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC descRenderTargetHeap {};
		descRenderTargetHeap.NumDescriptors = FRAME_BUFFER_COUNT;
		descRenderTargetHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		descRenderTargetHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = m_d3dDevice->CreateDescriptorHeap(&descRenderTargetHeap, IID_PPV_ARGS(&m_d3dDescTarget));
		if (FAILED(hr))
			return hr;

		D3D12_DESCRIPTOR_HEAP_DESC descDepthStencilHeap{};
			descDepthStencilHeap.NumDescriptors = 1;
			descDepthStencilHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			descDepthStencilHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = m_d3dDevice->CreateDescriptorHeap(&descDepthStencilHeap, IID_PPV_ARGS(&m_d3dDescDepth));
		if (FAILED(hr))
			return hr;
	}

	// 6. Create frame resources.
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_d3dDescTarget->GetCPUDescriptorHandleForHeapStart();
		// Create a RTV for each frame.
		for (UINT n = 0; n < FRAME_BUFFER_COUNT; n++)
		{
			hr = m_d3dSwapChain->GetBuffer(n, IID_PPV_ARGS(&m_d3dRenderTarget[n]));
			if (FAILED(hr))
				return hr;
			m_d3dDevice->CreateRenderTargetView(m_d3dRenderTarget[n].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += m_d3dSizeDescRenderTarget;
		}
	}

	// 7. Create a depth stencil and view.
	{
		// 1. 깊이 스텐실 뷰(DSV) 설명 구조체 설정
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_formatDepthBuffer;                          // 예: DXGI_FORMAT_D32_FLOAT
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;        // 2D 텍스처로 사용
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		// 7.2. 힙 프로퍼티 설정: 기본 힙 (GPU 전용 메모리)
		D3D12_HEAP_PROPERTIES depthHeapProperties = {};
		depthHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		depthHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		depthHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		depthHeapProperties.CreationNodeMask = 1;
		depthHeapProperties.VisibleNodeMask = 1;

		// 7.3. 깊이 버퍼 리소스 설명
		D3D12_RESOURCE_DESC depthResourceDesc = {};
		depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthResourceDesc.Alignment = 0;
		depthResourceDesc.Width = m_screenSize.cx;
		depthResourceDesc.Height = m_screenSize.cy;
		depthResourceDesc.DepthOrArraySize = 1;
		depthResourceDesc.MipLevels = 1;
		depthResourceDesc.Format = m_formatDepthBuffer;                // 예: DXGI_FORMAT_D32_FLOAT
		depthResourceDesc.SampleDesc.Count = 1;
		depthResourceDesc.SampleDesc.Quality = 0;
		depthResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		// 7.4. 깊이 버퍼 클리어 값
		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = m_formatDepthBuffer;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		// 7.5. 깊이 버퍼 리소스 생성
		hr = m_d3dDevice->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_d3dDepthStencil)
		);
		if (FAILED(hr))
			return hr;

		// 7.6. 깊이 스텐실 뷰 생성
		m_d3dDevice->CreateDepthStencilView(m_d3dDepthStencil.Get(), &dsvDesc, m_d3dDescDepth->GetCPUDescriptorHandleForHeapStart() );
	}

	for (UINT n = 0; n < FRAME_BUFFER_COUNT; n++)
	{
		m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_d3dCommandAllocator[n]));
		if (FAILED(hr))
			return hr;
	}
	// Create the command list.
	auto commandAlloc = std::any_cast<ID3D12CommandAllocator*>(this->GetCommandAllocator());
	hr = m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAlloc, nullptr, IID_PPV_ARGS(&m_d3dCommandList));
	if (FAILED(hr))
		return hr;

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	hr = m_d3dCommandList->Close();
	if (FAILED(hr))
		return hr;

	// Create synchronization objects.
	{
		hr = m_d3dDevice->CreateFence(m_fenceValue[m_d3dCurrentFrameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
		if (FAILED(hr))
			return hr;
		m_fenceValue[m_d3dCurrentFrameIndex]++;

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
	WaitForGpu();

	Destroy();

	CloseHandle(m_fenceEvent);

	m_fence				.Reset();
	m_d3dCommandQueue	.Reset();
	m_d3dDescTarget		.Reset();
	for(int i=0; i<FRAME_BUFFER_COUNT; ++i)
	{
		m_d3dRenderTarget		[i].Reset();
		m_d3dCommandAllocator	[i].Reset();
	}
	m_d3dPipelineState		.Reset();
	m_d3dCommandList		.Reset();
	m_d3dSwapChain			.Reset();
	G2::SAFE_RELEASE(m_d3dDevice);
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
	//auto commandAlloc = std::any_cast<ID3D12CommandAllocator*>(this->GetCommandAllocator());

	//// Command allocator는 GPU가 해당 작업을 마친 후에만 Reset 가능.
	//hr = commandAlloc->Reset();
	//if (FAILED(hr))
	//	return hr;

	//
	//// Command list는 Execute 후 언제든 Reset 가능. 반드시 Reset 후 재사용해야 함.
	//hr = m_d3dCommandList->Reset(commandAlloc, m_d3dPipelineState.Get());
	//if (FAILED(hr))
	//	return hr;

	//// --- 1. 리소스 상태 전환: Present → RenderTarget ---
	//D3D12_RESOURCE_BARRIER barrier = {};
	//barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//barrier.Transition.pResource = m_d3dRenderTarget[m_d3dCurrentFrameIndex].Get();
	//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//m_d3dCommandList->ResourceBarrier(1, &barrier);

	//// --- 2. render target and depth handle
	//D3D12_CPU_DESCRIPTOR_HANDLE descRenderTargetView = m_d3dDescTarget->GetCPUDescriptorHandleForHeapStart();
	//descRenderTargetView.ptr += static_cast<SIZE_T>(m_d3dCurrentFrameIndex) * static_cast<SIZE_T>(m_d3dSizeDescRenderTarget);
	//D3D12_CPU_DESCRIPTOR_HANDLE descDepthView = m_d3dDescDepth->GetCPUDescriptorHandleForHeapStart();

	//// --- 3. clear render target and depth handle
	//const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	//m_d3dCommandList->ClearRenderTargetView(descRenderTargetView, clearColor, 0, nullptr);
	//m_d3dCommandList->ClearDepthStencilView(descDepthView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//// --- 4. binding render target and depth handle
	//m_d3dCommandList->OMSetRenderTargets(1, &descRenderTargetView, FALSE, &descDepthView);
	//m_d3dCommandList->SetGraphicsRootSignature(m_d3dRootSignature.Get());

	//// --- 5. set viewport and scissor rect
	//m_d3dCommandList->RSSetViewports(1, &m_d3dViewport);
	//m_d3dCommandList->RSSetScissorRects(1, &m_d3dScissorRect);


	//
	
	//

	// --- 4. 리소스 상태 전환: RenderTarget → Present ---
	//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	//m_d3dCommandList->ResourceBarrier(1, &barrier);

	//// --- 5. 명령 리스트 종료 ---
	//hr = m_d3dCommandList->Close();

	return hr;
}

HRESULT D3DApp::WaitForGpu()
{
	HRESULT hr = S_OK;

	hr = m_d3dCommandQueue->Signal(m_fence.Get(), m_fenceValue[m_d3dCurrentFrameIndex]);
	if (FAILED(hr))
		return hr;
	hr = m_fence->SetEventOnCompletion(m_fenceValue[m_d3dCurrentFrameIndex], m_fenceEvent);
	if (FAILED(hr))
		return hr;

	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	m_fenceValue[m_d3dCurrentFrameIndex]++;
	m_d3dCurrentFrameIndex = m_d3dSwapChain->GetCurrentBackBufferIndex();
	return S_OK;
}

HRESULT D3DApp::MoveToNextFrame()
{
	HRESULT hr = S_OK;

	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_fenceValue[m_d3dCurrentFrameIndex];
	hr = m_d3dCommandQueue->Signal(m_fence.Get(), currentFenceValue);
	if (FAILED(hr))
		return hr;

	// Advance the frame index.
	m_d3dCurrentFrameIndex = m_d3dSwapChain->GetCurrentBackBufferIndex();

	// Check to see if the next frame is ready to start.
	if (m_fence->GetCompletedValue() < m_fenceValue[m_d3dCurrentFrameIndex])
	{
		hr = m_fence->SetEventOnCompletion(m_fenceValue[m_d3dCurrentFrameIndex], m_fenceEvent);
		if (FAILED(hr))
			return hr;
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	m_fenceValue[m_d3dCurrentFrameIndex] = currentFenceValue + 1;
	return S_OK;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_d3dDescTarget->GetCPUDescriptorHandleForHeapStart(), m_d3dCurrentFrameIndex, m_d3dSizeDescRenderTarget);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
	return m_d3dDescDepth->GetCPUDescriptorHandleForHeapStart();
}

int D3DApp::Init()
{
	m_pmain = new MainApp;
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