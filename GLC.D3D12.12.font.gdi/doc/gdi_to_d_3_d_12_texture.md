# GDI를 이용한 실시간 텍스트 렌더링 및 Direct3D 12 텍스처 업로드

이 문서는 Windows GDI를 활용하여 문자열을 그린 후, 해당 결과를 Direct3D 12의 텍스처로 업로드하는 전반적인 과정을 설명합니다.

---

## 문자열 크기 측정
```cpp
SIZE GetStringSize(const string& text, HFONT hFont)
{
    HDC hDC = GetDC(nullptr);
    SIZE size{};
    auto oldFont = (HFONT)SelectObject(hDC, hFont);
    GetTextExtentPoint32A(hDC, text.c_str(), (int)text.length(), &size);
    SelectObject(hDC, oldFont);
    ReleaseDC(nullptr, hDC);
    return size;
}
```

---

## 텍스트 텍스처 생성 함수
```cpp
int CreateTextureFromText(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, 
                           const string& text, ComPtr<ID3D12Resource>& outTexture)
{
    HRESULT hr = S_OK;

    HFONT hFont = CreateFontA(
        32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial");

    SIZE textSize = GetStringSize(text, hFont);
    int width  = (textSize.cx + 3) / 4 * 4; // 4의 배수로 정렬
    int height = (textSize.cy + 3) / 4 * 4;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // BGRA
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hdcMem = CreateCompatibleDC(nullptr);
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    SelectObject(hdcMem, hBitmap);
    memset(pBits, 0x00, width * height * 4); // 검정 배경 초기화

    auto oldFont = (HFONT)SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 255));
    TextOutA(hdcMem, 0, 0, text.c_str(), (int)text.length());
    SelectObject(hdcMem, oldFont);

    // 알파 채널 설정: RGB 값이 0 (검정)이면 투명, 아니면 불투명
    unique_ptr<uint32_t[]> pixelCopy(new uint32_t[width * height]);
    memcpy(pixelCopy.get(), pBits, width * height * 4);

    for (int i = 0; i < width * height; ++i)
    {
        auto& px = pixelCopy[i];
        uint8_t b = (px >> 0) & 0xFF;
        uint8_t g = (px >> 8) & 0xFF;
        uint8_t r = (px >> 16) & 0xFF;
        uint8_t a = (r + g + b) / 3;
        px = (px & 0x00FFFFFF) | (a << 24); // 알파 덮어쓰기
    }

    DeleteObject(hBitmap);
    DeleteObject(hFont);
    DeleteDC(hdcMem);

    // 텍스처 리소스 생성 (GPU)
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES heapDefault(D3D12_HEAP_TYPE_DEFAULT);
    hr = device->CreateCommittedResource(
        &heapDefault,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&outTexture));

    // 업로드 버퍼 생성
    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

    ComPtr<ID3D12Resource> uploadBuf;
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    CD3DX12_HEAP_PROPERTIES heapUpload(D3D12_HEAP_TYPE_UPLOAD);

    hr = device->CreateCommittedResource(
        &heapUpload,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuf));

    // 업로드 수행
    D3D12_SUBRESOURCE_DATA sub = {};
    sub.pData = pixelCopy.get();
    sub.RowPitch = width * 4;
    sub.SlicePitch = width * height * 4;

    UpdateSubresources<1>(cmdList, outTexture.Get(), uploadBuf.Get(), 0, 0, 1, &sub);
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        outTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &barrier);

    return S_OK;
}
```
## 결과
- <img width="320" alt="directx12_hangul_gdi_to_texture" src="https://github.com/user-attachments/assets/97e6e931-05a9-46ce-bd89-849d35031657" />

---

## 요약 흐름

1. `CreateFontA()`로 GDI 폰트 생성
2. `CreateDIBSection()`으로 BGRA 32비트 버퍼 확보
3. `TextOut()`으로 텍스트 렌더링
4. 픽셀 복사 및 알파 채널 설정
5. `CreateCommittedResource()`로 GPU 텍스처 생성
6. `UpdateSubresources()`로 GPU로 업로드
7. `ResourceBarrier()`로 PIXEL_SHADER_RESOURCE 상태 전환
