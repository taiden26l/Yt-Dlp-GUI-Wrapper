#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "core/Settings.h"
#include "download/DownloadManager.h"
#include "ui/MainWindow.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

ID3D11Device* gD3DDevice = nullptr;
ID3D11DeviceContext* gD3DContext = nullptr;
IDXGISwapChain* gSwapChain = nullptr;
ID3D11RenderTargetView* gMainRenderTargetView = nullptr;

void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(gSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        gD3DDevice->CreateRenderTargetView(backBuffer, nullptr, &gMainRenderTargetView);
        backBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (gMainRenderTargetView != nullptr) {
        gMainRenderTargetView->Release();
        gMainRenderTargetView = nullptr;
    }
}

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    if (D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        levels,
        2,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &gSwapChain,
        &gD3DDevice,
        &featureLevel,
        &gD3DContext) != S_OK) {
        return false;
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (gSwapChain != nullptr) {
        gSwapChain->Release();
        gSwapChain = nullptr;
    }
    if (gD3DContext != nullptr) {
        gD3DContext->Release();
        gD3DContext = nullptr;
    }
    if (gD3DDevice != nullptr) {
        gD3DDevice->Release();
        gD3DDevice = nullptr;
    }
}

LRESULT WINAPI WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
        return TRUE;
    }

    switch (message) {
    case WM_SIZE:
        if (gD3DDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            gSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

} 
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"YtDlpDesktop";
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&windowClass);

    HWND hwnd = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"yt-dlp Desktop",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        1180,
        760,
        nullptr,
        nullptr,
        instance,
        nullptr);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(windowClass.lpszClassName, instance);
        CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(gD3DDevice, gD3DContext);

    core::AppSettings settings = core::SettingsStore::Load();
    download::DownloadManager downloadManager;
    downloadManager.Start();
    downloadManager.SetYtDlpPath(settings.ytDlpPath);
    downloadManager.RefreshDependencies();

    ui::MainWindow mainWindow(hwnd, settings, downloadManager);

    MSG message{};
    while (message.message != WM_QUIT) {
        if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        mainWindow.Render();
        ImGui::Render();

        const float clearColor[4] = { 0.09f, 0.09f, 0.10f, 1.0f };
        gD3DContext->OMSetRenderTargets(1, &gMainRenderTargetView, nullptr);
        gD3DContext->ClearRenderTargetView(gMainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        gSwapChain->Present(1, 0);
    }

    core::SettingsStore::Save(settings);
    downloadManager.Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(windowClass.lpszClassName, instance);
    CoUninitialize();
    return 0;
}
