// Interface for the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _D3DApp_H_
#define _D3DApp_H_

#include <string>
#include <Windows.h>

#define APP_WIN_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE)

class D3DApp
{
public:
	static D3DApp*	getInstance();								// sigleton
	int		Create(HINSTANCE hInst);
	int		Run();
	void	Cleanup();
	int		Render();
	static	LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);	// window proc function

protected:
	D3DApp();
	static D3DApp*		m_pAppMain;								// sigleton Instance
	const std::string	m_class = "GLC.D3D.Main.Class";
	std::string			m_name;									// windows class name
	HINSTANCE			m_hInst			{};						//
	HWND				m_hWnd			{};
	DWORD				m_dWinStyle		{ APP_WIN_STYLE };
	::SIZE				m_screenSize	{ 1280, 720 };			// HD Screen size width, height
	bool				m_showCusor		{ true };				// Show Cusor
	bool				m_bWindow		{ true };				// Window mode
	LRESULT MsgProc(HWND, UINT, WPARAM, LPARAM);
};

#endif
