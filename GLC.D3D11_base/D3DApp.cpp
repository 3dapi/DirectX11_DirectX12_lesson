// Implementation of the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#include "D3DApp.h"

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

	return ERROR_SUCCESS;
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
	return ERROR_SUCCESS;
}
