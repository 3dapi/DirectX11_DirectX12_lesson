#pragma once
#ifndef _MainApp_H_
#define _MainApp_H_

#include <vector>
#include <d3d11.h>
#include <DirectxMath.h>

using DirectX::XMMATRIX;
using DirectX::XMFLOAT4;

class MainApp
{
protected:
	ID3D11VertexShader*		m_shaderVtx		{};
	ID3D11PixelShader*		m_shaderPxl		{};
	ID3D11InputLayout*		m_vtxLayout		{};
	ID3D11Buffer*			m_bufVtx		{};
	int						m_bufVtxCount	{};
	ID3D11Buffer*			m_bufIdx		{};
	int						m_bufIdxCount	{};
	ID3D11Buffer*			m_cnstWorld		{};
	ID3D11Buffer*			m_cnstView		{};
	ID3D11Buffer*			m_cnstProj		{};
	ID3D11Buffer*			m_cnstDiff		{};

	ID3D11ShaderResourceView*		m_srvTexture	{};
	ID3D11SamplerState*				m_sampLinear	{};

	XMMATRIX				m_mtView		{};
	XMMATRIX				m_mtProj		{};
	XMMATRIX				m_mtWorld		{};
	XMFLOAT4				m_difMesh		{0.7F, 0.7F, 0.7F, 1.0F};
public:
	MainApp();
	virtual ~MainApp();
	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();
};


#endif
