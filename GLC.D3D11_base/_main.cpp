
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
