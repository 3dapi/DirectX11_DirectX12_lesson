// Implementation of the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#include <any>
#include <algorithm>
#include <cassert>
#include <memory>
#include <Windows.h>
#include <winerror.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include "G2Base.h"
#include "G2Util.h"
#include "DDSTextureLoader.h"

std::wstring G2::StringToWString(const std::string& str)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, {}, 0);
	std::wstring wstr(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
	return wstr;
}

HRESULT G2::DXCompileShaderFromFile(const std::string& szFileName, const std::string& szShaderModel, const std::string& szEntryPoint, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	auto wFileName = G2::StringToWString(szFileName);
	ID3DBlob* pErrorBlob {};
	hr = D3DCompileFromFile(wFileName.c_str(), {}, {}, szEntryPoint.c_str(), szShaderModel.c_str(), dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			SAFE_RELEASE(pErrorBlob);
		}
		return hr;
	}
	SAFE_RELEASE(pErrorBlob);

	return S_OK;
}

std::tuple<HRESULT, ID3D11ShaderResourceView*, ID3D11Resource*>
G2::DXCreateDDSTextureFromFile(const std::string& szFileName, bool mipMap)
{
	HRESULT						ret_hr{};
	ID3D11ShaderResourceView*	ret_srv{};
	ID3D11Resource*				ret_rsc{};
	auto wFileName = G2::StringToWString(szFileName);
	auto d3dDevice = std::any_cast<ID3D11Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());
	ret_hr = DirectX::CreateDDSTextureFromFile(d3dDevice, mipMap? d3dContext: nullptr, wFileName.c_str(), &ret_rsc, &ret_srv);
	return std::make_tuple(ret_hr, ret_srv, ret_rsc );
}
