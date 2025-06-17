
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "comctl32")


#include "D3DApp.h"

int main(int argc, char** argv)
{
	HINSTANCE hInst  = GetModuleHandle(nullptr);
	auto pMain = D3DApp::getInstance();
	if (FAILED(pMain->Create(hInst)))
		return 0;
	pMain->Run();
	return 0;
}
