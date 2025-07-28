// Implementation of the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#include <any>
#include <algorithm>
#include <cassert>
#include <filesystem>
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
#include "WICTextureLoader.h"

std::string G2::toLower(const std::string& str_t) {
	std::string ret = str_t;
	std::transform(ret.begin(), ret.end(), ret.begin(),
				   [](unsigned char c) { return std::tolower(c); });
	return ret;
}

std::wstring G2::ansiToWstr(const std::string& str)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, {}, 0);
	std::wstring wstr(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
	return wstr;
}

void G2::avx2_memcpy(void* dst, const void* src, size_t size)
{
	// 작은 데이터는 memcpy로 처리 (성능 유리)
	if(size < 1024)
	{
		std::memcpy(dst,src,size);
		return;
	}

	uint8_t* d = reinterpret_cast<uint8_t*>(dst);
	const uint8_t* s = reinterpret_cast<const uint8_t*>(src);

	size_t simdSize = size & ~31ULL;  // 32바이트 배수로 정렬
	size_t remain   = size &  31ULL;  // 나머지 바이트

	// --- AVX2 복사 (32바이트 단위) ---
	for(size_t i = 0; i < simdSize; i += 32)
	{
		__m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i));
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(d + i),v);
	}

	// --- 남은 바이트는 memcpy로 처리 ---
	if(remain > 0)
	{
		std::memcpy(d + simdSize,s + simdSize,remain);
	}
}

void G2::avx2_memset32(void* dst, int32_t val, size_t count)
{
	int32_t* d = reinterpret_cast<int32_t*>(dst);

	if(count * sizeof(int32_t) < 1024)
	{
		for(size_t i = 0; i < count; ++i)
			d[i] = val;
		return;
	}

	size_t simdCount = count & ~7ULL;  // 8개씩 처리 (32B)
	size_t remain    = count & 7ULL;

	__m256i vec = _mm256_set1_epi32(val);  // 8 x int32_t

	for(size_t i = 0; i < simdCount; i += 8)
	{
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(d + i),vec);
	}

	for(size_t i = simdCount; i < count; ++i)
	{
		d[i] = val;
	}
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
	auto wFileName = G2::ansiToWstr(szFileName);
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
G2::DXCreateTextureFromFile(const std::string& szFileName, bool mipMap)
{
	HRESULT						ret_hr{};
	ID3D11ShaderResourceView*	ret_srv{};
	ID3D11Resource*				ret_rsc{};
	std::filesystem::path str_path(szFileName);
	auto ext = G2::toLower(str_path.extension().generic_string());

	auto wFileName = G2::ansiToWstr(szFileName);
	auto d3dDevice = std::any_cast<ID3D11Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	auto d3dContext = std::any_cast<ID3D11DeviceContext*>(IG2GraphicsD3D::getInstance()->GetContext());

	if (0 == ext.compare(".dds"))
	{
		ret_hr = DirectX::CreateDDSTextureFromFile(d3dDevice, mipMap? d3dContext: nullptr, wFileName.c_str(), &ret_rsc, &ret_srv);
	}
	else
	{
		ret_hr = DirectX::CreateWICTextureFromFile(d3dDevice, mipMap? d3dContext: nullptr, wFileName.c_str(), &ret_rsc, &ret_srv);
	}
	return std::make_tuple(ret_hr, ret_srv, ret_rsc );

	
}
