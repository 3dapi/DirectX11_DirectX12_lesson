// Interface for the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _D3DApp_H_
#define _D3DApp_H_

#include <string>
#include <vector>
#include <Windows.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#include "G2Base.h"

using Microsoft::WRL::ComPtr;


#define APP_WIN_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE)

class D3DApp : public IG2GraphicsD3D
{
public:
	int			Create(void* initialist) override;
	int			Run() override;
	std::any	GetAttrib(int nCmd) override;
	std::any	GetDevice() override;
	std::any	GetContext() override;

	static D3DApp*	getInstance();								// sigleton
	void			Cleanup();
	int				RenderApp();
	static	LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);	// window proc function

	// window settings
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

	// device and context
protected:
	ID3D11Device*			m_d3dDevice				{};
	ID3D11DeviceContext*	m_d3dContext			{};
	IDXGISwapChain*			m_d3dSwapChain			{};
	ID3D11Device1*			m_d3dDevice_1			{};
	ID3D11DeviceContext1*	m_d3dContext_1			{};
	IDXGISwapChain1*		m_d3dSwapChain_1		{};
	ID3D11RenderTargetView*	m_d3dRenderTargetView	{};
	ID3D11Texture2D*		m_d3dDepthStencil		{};
	ID3D11DepthStencilView*	m_d3dDepthStencilView	{};
	D3D_FEATURE_LEVEL		m_featureLevel			{};
	int		InitDevice();
	int		ReleaseDevice();

	// for driven class
public:
	virtual int Init()		;
	virtual int Destroy()	;
	virtual int Update()	;
	virtual int Render()	;

	class MainApp* m_pmain{};
};

#endif
