#include <windows.h>
#include <gdiplus.h>
#include <cstdio>
#pragma comment(lib, "gdiplus.lib")

#pragma pack(push, 2)
typedef struct {
    WORD idReserved;
    WORD idType;
    WORD idCount;
} ICONDIR;

typedef struct {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
} ICONDIRENTRY;
#pragma pack(pop)

int main() {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Load the PNG
    Gdiplus::Bitmap bitmap(L"browser_icon.ico");
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        printf("Failed to load PNG\n");
        return 1;
    }

    // Resize to 32x32
    Gdiplus::Bitmap resized(32, 32, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&resized);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQuality);
    graphics.DrawImage(&bitmap, 0, 0, 32, 32);

    // Create ICO header
    ICONDIRENTRY dirEntry = {};
    dirEntry.bWidth = 32;
    dirEntry.bHeight = 32;
    dirEntry.bColorCount = 0;
    dirEntry.bReserved = 0;
    dirEntry.wPlanes = 1;
    dirEntry.wBitCount = 32;
    dirEntry.dwBytesInRes = 40 + 32 * 32 * 4 + 128;
    dirEntry.dwImageOffset = 22;

    ICONDIR iconDir = {};
    iconDir.idReserved = 0;
    iconDir.idType = 1;
    iconDir.idCount = 1;

    // Get bitmap data
    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, 32, 32);
    resized.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData);

    BITMAPINFOHEADER bmi = {};
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biWidth = 32;
    bmi.biHeight = 64;
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;

    FILE* f = fopen("browser_icon.ico", "wb");
    if (!f) {
        printf("Failed to create output file\n");
        return 1;
    }

    // Write header
    fwrite(&iconDir, 6, 1, f);
    fwrite(&dirEntry, 16, 1, f);
    fwrite(&bmi, 40, 1, f);

    // Write bitmap data (AND mask then XOR mask)
    BYTE* pixels = (BYTE*)bitmapData.Scan0;
    for (int y = 31; y >= 0; y--) {
        for (int x = 0; x < 32; x++) {
            int idx = y * bitmapData.Stride + x * 4;
            fwrite(&pixels[idx + 2], 1, 1, f);
            fwrite(&pixels[idx + 1], 1, 1, f);
            fwrite(&pixels[idx + 0], 1, 1, f);
            fwrite(&pixels[idx + 3], 1, 1, f);
        }
    }

    // Write AND mask (all zeros for transparent)
    BYTE andMask[128] = {0};
    fwrite(andMask, 128, 1, f);

    fclose(f);
    resized.UnlockBits(&bitmapData);
    Gdiplus::GdiplusShutdown(gdiplusToken);

    printf("Icon converted successfully\n");
    return 0;
}
