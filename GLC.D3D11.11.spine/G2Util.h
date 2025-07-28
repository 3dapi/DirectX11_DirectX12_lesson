// Interface for the D3DUtil class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _G2Util_H_
#define _G2Util_H_

#include <string>
#include <tuple>
#include <vector>
#include <Windows.h>
#include <d3dcommon.h>
#include <d3d11.h>

namespace G2 {
template<typename T>
inline void SAFE_DELETE(T*& p)
{
	if (p)
	{
		delete p;
		p = {};
	}
}
template<typename T>
inline void SAFE_DELETE_ARR(T*& p)
{
	if (p)
	{
		delete[] p;
		p = {};
	}
}

template<typename T>
inline void SAFE_RELEASE(T*& p)
{
	if (p)
	{
		p->Release();
		p = {};
	}
}
// NOTE: for new T*[count] raw pointer array
template<typename T>
inline void SAFE_RELEASE_ARRAY(T*& p, size_t count)
{
	if (p)
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (p[i])
				p[i]->Release();
		}
		delete[] p;
		p = {};
	}
}
// NOTE: for std::vector 
template<typename T>
inline void SAFE_RELEASE_VECTOR(std::vector<T*>& vec) {
	for (auto& p : vec) {
		if (p) {
			p->Release();
			p = {};
		}
	}
	vec.clear();
}

std::string toLower(const std::string& str_t);
std::wstring ansiToWstr(const std::string& str);
void        avx2_memcpy(void* dst, const void* src, size_t size);	// avx2 메모리 복사: memcpy 대신
void        avx2_memset32(void* dst, int32_t val, size_t count);	// 32bit 값 채우기: memset 대신.
HRESULT		DXCompileShaderFromFile(const std::string& szFileName, const std::string& szShaderModel, const std::string& szEntryPoint, ID3DBlob** ppBlobOut);

std::tuple<HRESULT, ID3D11ShaderResourceView*, ID3D11Resource*>
			DXCreateTextureFromFile(const std::string& szFileName, bool mipMap= true);
} // namespace G2
#endif
