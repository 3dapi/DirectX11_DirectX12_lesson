#pragma once
#ifndef _MainApp_H_
#define _MainApp_H_

#include <d3d11.h>

class MainApp
{
protected:
	ID3D11VertexShader*		m_shaderVtx		{};
	ID3D11PixelShader*		m_shaderPxl		{};
	ID3D11InputLayout*		m_vtxLayout		{};
	ID3D11Buffer*			m_vtxBuffer		{};
	int						m_vtxCount		{};
public:
	MainApp();
	virtual ~MainApp();
	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();
};


#endif
