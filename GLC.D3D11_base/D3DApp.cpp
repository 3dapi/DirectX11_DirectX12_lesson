// Implementation of the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#include "D3DApp.h"

D3DApp* D3DApp::g_pMainApp = {};
D3DApp::D3DApp()
{
	D3DApp::g_pMainApp = this;
	m_name = "DirectX 11 App";
}


LRESULT WINAPI D3DApp::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return g_pMainApp->MsgProc(hWnd, msg, wParam, lParam);
}

int D3DApp::Create(HINSTANCE hInst)
{
	m_hInst = hInst;

	WNDCLASS wc =								// Register the window class
	{
		CS_CLASSDC
		, WndProc
		, 0L
		, 0L
		, m_hInst
		, NULL
		, LoadCursor(NULL,IDC_ARROW)
		, (HBRUSH)GetStockObject(WHITE_BRUSH)
		, NULL
		, m_class.c_str()
	};

	RegisterClass(&wc);
	RECT rc{};									//Create the application's window

	SetRect(&rc, 0, 0, m_screenSize.cx, m_screenSize.cy);
	AdjustWindowRect(&rc, m_dWinStyle, FALSE);

	int iScnSysW = ::GetSystemMetrics(SM_CXSCREEN);
	int iScnSysH = ::GetSystemMetrics(SM_CYSCREEN);

	// window 생성.
	m_hWnd = CreateWindow(
						  m_class.c_str()
						, m_name.c_str()
						, m_dWinStyle
						, (iScnSysW - (rc.right - rc.left)) / 2
						, (iScnSysH - (rc.bottom - rc.top)) / 2
						, (rc.right - rc.left)
						, (rc.bottom - rc.top)
						, NULL
						, NULL
						, m_hInst
						, NULL
	);
	::ShowWindow(m_hWnd, SW_SHOW);
	::UpdateWindow(m_hWnd);
	::ShowCursor(m_showCusor);

	return S_OK;
}


void D3DApp::Cleanup()
{
}


int D3DApp::Run()
{
	MSG msg;
	memset(&msg, 0, sizeof(msg));

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}

	UnregisterClass(m_class.c_str(), m_hInst);

	return S_OK;
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
			SendMessage(hWnd, WM_DESTROY, 0, 0);
			break;
		}
		}

		return S_OK;

	}

	case WM_DESTROY:
	{
		Cleanup();
		PostQuitMessage(0);
		return S_OK;
	}
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

int D3DApp::Render()
{
	return S_OK;
}
