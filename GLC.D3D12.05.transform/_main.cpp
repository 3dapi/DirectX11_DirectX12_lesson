
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "comctl32")

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <Windows.h>
#include "G2Base.h"

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	//_CrtSetBreakAlloc(111111);
	auto pMain = IG2GraphicsD3D::getInstance();
	if (FAILED(pMain->Create({})))
		return 0;
	pMain->Run();
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}
