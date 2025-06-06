//
// cli_args_debugger.cpp
//
// Build Requirements:
// ------------------
// 1. Visual Studio 2022 (Community Edition or higher)
//    - Install the following workloads:
//         * "Desktop development with C++"
//         * "Universal Windows Platform development"
// 2. Required components:
//    - Windows 10/11 SDK (latest version)
//    - Visual C++ tools for CMake
// 3. DirectX Dependencies:
//    - DirectX SDK (June 2010) for legacy components, or a modern Windows SDK
//
// Additionally: Place the files qrcodegen.hpp and qrcodegen.cpp (from QrCodeGen by Nayuki)
// in the same directory as this file and add them to your project.
// ------------------

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <cstdint>
#include <array>
#include <shlobj.h>
#include <knownfolders.h>
#include <cstdio>
#include <fstream>
#include <atomic>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <algorithm>
#include <cmath>
#include <avrt.h>  // For AvSetMmThreadCharacteristics
#include <iomanip> // For setw/setfill
#include <excpt.h> // For SEH exception handling
#include <share.h> // For _wfsopen share flags
#define INITGUID   // Required by /permissive- for GUID initialization
#include <ks.h>    // For GUID constants
#include <ksmedia.h> // For KSDATAFORMAT_SUBTYPE constants
#include <functiondiscoverykeys_devpkey.h>  // For PKEY_Device_FriendlyName
#include <propsys.h>          // IPropertyStore, PKEY_*
#include <propvarutil.h>      // PropVariant helpers
#pragma comment(lib, "propsys")   // + linker

// Link with required libraries
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "ole32")
#pragma comment(lib, "avrt")
#pragma comment(lib, "user32")   // For window functions
#pragma comment(lib, "shell32")  // For SHGetKnownFolderPath
#pragma comment(lib, "gdi32")    // For GDI functions

// Include QrCodeGen (ensure that qrcodegen.hpp and qrcodegen.cpp are in your project)
#include "qrcodegen.hpp"
using qrcodegen::QrCode;

// Use Microsoft::WRL::ComPtr for COM object management
using Microsoft::WRL::ComPtr;

// Helper macro that calls a DirectX function and throws an exception on failure.
#define DX_CALL(expr, msg) do { auto hr = (expr); if (FAILED(hr)) throw std::runtime_error(msg); } while(0)

// ===========================================================================
//  LOGGING SUPPORT
// ===========================================================================

// Simple header-only logger that writes UTF-16 text to a file
// located next to the executable (ArgumentDebugger.log).
//
//  • InitLogger()   – call once from wWinMain right after COM is up.
//  • Log(L"text")  – append one line with local‑time prefix.
//  • ShowLogs()    – read the file and place it into loaded_data_ so it is
//                    rendered in the right‑upper corner (triggered via "logs")

namespace                               // anonymous, internal
{
    FILE* g_log_file = nullptr;        // File handle for the log file
    std::wstring g_logPath;            // Path to the log file
    CRITICAL_SECTION g_log_cs;         // Critical section for thread safety

    void InitLogger()
    {
        wchar_t exePath[MAX_PATH] = L"\0";
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        g_logPath.assign(exePath);
        size_t pos = g_logPath.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            g_logPath.erase(pos + 1);
        g_logPath += L"ArgumentDebugger.log";

        // Open file in append mode with UTF-16LE encoding
        // Use _SH_DENYNO to allow other handles to read the file while we have it open
        g_log_file = _wfsopen(g_logPath.c_str(), 
                              L"a+, ccs=UTF-16LE", 
                              _SH_DENYNO);
                              
        // If this is a new file, write BOM (Byte Order Mark)
        if (g_log_file && ftell(g_log_file) == 0) {
            fputwc(0xFEFF, g_log_file); // UTF-16LE BOM
        }
        
        // Initialize critical section for thread-safe logging
        InitializeCriticalSection(&g_log_cs);
    }

    void Log(const std::wstring& text)
    {
        if (!g_log_file) return;
        
        // Lock the critical section before accessing the file
        EnterCriticalSection(&g_log_cs);
        
        SYSTEMTIME st; 
        GetLocalTime(&st);
        
        // Format log entry with timestamp
        std::wstring entry = L"[";
        entry += std::to_wstring(st.wYear) + L"-";
        
        // Month with padding
        if (st.wMonth < 10) entry += L"0";
        entry += std::to_wstring(st.wMonth) + L"-";
        
        // Day with padding
        if (st.wDay < 10) entry += L"0";
        entry += std::to_wstring(st.wDay) + L" ";
        
        // Hour with padding
        if (st.wHour < 10) entry += L"0";
        entry += std::to_wstring(st.wHour) + L":";
        
        // Minute with padding
        if (st.wMinute < 10) entry += L"0";
        entry += std::to_wstring(st.wMinute) + L":";
        
        // Second with padding
        if (st.wSecond < 10) entry += L"0";
        entry += std::to_wstring(st.wSecond) + L"] ";
        
        // Add the actual log message
        entry += text + L"\n";
        
        // Write to file and flush
        fputws(entry.c_str(), g_log_file);
        fflush(g_log_file);
        
        // Unlock the critical section
        LeaveCriticalSection(&g_log_cs);
    }
}

// Simple log function for SEH wrapper to use
// Exported for seh_wrapper.cpp
void LogSEH(const wchar_t* message)
{
    if (message) {
        // Convert C-string to wstring for main Log function
        Log(std::wstring(message));
        // Also output to debug console for immediate visibility
        OutputDebugStringW(message);
        OutputDebugStringW(L"\n");
    }
}

// Helper function: robust conversion from std::wstring to std::string using WideCharToMultiByte (UTF-8)
std::string wstring_to_string(const std::wstring &wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                          static_cast<int>(wstr.size()),
                                          nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                        static_cast<int>(wstr.size()),
                        &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

#include "cli_args_display.hpp"

constexpr const wchar_t* kWindowClassName = L"ArgumentDebuggerClass";
constexpr const wchar_t* kWindowCaption   = L"Argument Debugger";

// Description text for the window
const std::vector<std::wstring> kDescriptionLines = {
    L"Argument Debugger",
    L"This utility displays all command-line arguments in a full-screen window.",
    L"Type 'exit', 'save', 'read' or 'logs' and press Enter to execute commands."
};

constexpr float kMargin     = 20.0f;
constexpr float kLineHeight = 30.0f;

// Structures for 3D rendering
struct SimpleVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
};

struct ConstantBufferData {
    DirectX::XMMATRIX world_view_projection;
};

class ArgumentDebuggerWindow;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
ArgumentDebuggerWindow* g_app_instance = nullptr;

// External SEH wrapper function (defined in seh_wrapper.cpp)
extern "C" DWORD WINAPI RawAudioThreadWithSEH(LPVOID param) noexcept;

// Global function for unhandled exception filter - renamed to avoid conflict
LONG WINAPI AppUnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    Log(L"Unhandled SEH exception: code=0x" + std::to_wstring(ep->ExceptionRecord->ExceptionCode));
    return EXCEPTION_EXECUTE_HANDLER;
}

class ArgumentDebuggerWindow {
public:
    ArgumentDebuggerWindow() = default;
    ~ArgumentDebuggerWindow() { Cleanup(); }

    // Throws on failure.
    void Initialize(HINSTANCE h_instance, int cmd_show, const std::vector<std::wstring>& args);
    int RunMessageLoop();
    void OnCharInput(wchar_t ch);
    void OnDestroy();
    bool is_running() const { return is_running_; }
    
    // Thread implementation with C++ exception handling - public for thread access
    DWORD AudioCaptureThreadImpl(LPVOID param);

private:
    void InitializeWindow(HINSTANCE h_instance, int cmd_show);
    void InitializeDevice();
    void CreateDeviceAndSwapChain(UINT width, UINT height);
    void CreateRenderTargetView();
    void CreateD2DResources();
    void CreateShadersAndGeometry();
    void InitializeMicrophone();
    void PollMicrophone();
    void RenderFrame();
    void Cleanup();
    void UpdateRotation(float delta_time);
    
    // Audio capture thread function with correct calling convention and SEH wrapper
    static __declspec(nothrow) DWORD WINAPI AudioCaptureThread(LPVOID param);
    
private:

    // Update QR code – here we add the FPS synchronization logic.
    void UpdateQrCode(ULONGLONG current_time);

    // Methods for file operations (saving/loading data)
    void SaveData();
    void ReadData();
    
    // Helper to load entire log file into loaded_data_
    void ShowLogs();

    HWND window_handle_ = nullptr;
    bool is_running_ = true;
    ULONGLONG last_time_ = 0;
    float rotation_angle_ = 0.0f;
    std::wstring user_input_;
    std::vector<std::wstring> args_;

    // New variables for displaying command status and loaded data:
    std::wstring command_status_;
    std::wstring loaded_data_;

    // Variables for FPS and QR code
    float current_fps_ = 0.0f;
    // Adding a variable for "synchronized" FPS,
    // which will be the same as what is shown in the QR code.
    int synced_fps_ = 0;

    ULONGLONG last_qr_update_time_ = 0;
    ComPtr<ID2D1Bitmap> qr_bitmap_;

    // Direct2D and DirectWrite objects
    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<ID2D1RenderTarget> d2d_render_target_;
    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextFormat> small_text_format_; // Smaller font for logs

    // Direct3D objects
    ComPtr<ID3D11Device> d3d_device_;
    ComPtr<ID3D11DeviceContext> immediate_context_;
    ComPtr<IDXGISwapChain> swap_chain_;
    ComPtr<ID3D11RenderTargetView> d3d_render_target_view_;

    // Shader and geometry objects
    ComPtr<ID3D11Buffer> vertex_buffer_;
    ComPtr<ID3D11Buffer> index_buffer_;
    ComPtr<ID3D11Buffer> constant_buffer_;
    ComPtr<ID3D11InputLayout> vertex_layout_;
    ComPtr<ID3D11VertexShader> vertex_shader_;
    ComPtr<ID3D11PixelShader> pixel_shader_;

    // WASAPI
    ComPtr<IMMDeviceEnumerator>   device_enumerator_;
    ComPtr<IMMDevice>             capture_device_;
    ComPtr<IAudioClient>          audio_client_;
    ComPtr<IAudioCaptureClient>   capture_client_;
    WAVEFORMATEX*                 mix_format_ = nullptr;
    HANDLE                        audio_event_ = nullptr;
    HANDLE                        audio_thread_ = nullptr;
    std::atomic<float>            mic_level_{0.f};   // 0..1
    std::atomic<bool>             mic_available_{false};
    std::atomic<bool>             audio_thread_running_{false};  // Flag for safe thread termination
    std::wstring                  mic_name_;   // FriendlyName ("USB Mic (Realtek ...)")
};

int WINAPI wWinMain(HINSTANCE h_instance, HINSTANCE, PWSTR, int cmd_show) {
    try {
        // Initialize COM once at the start - STA is the safest option for UI thread and D2D
        DX_CALL(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED),
                "COM initialization failed");
        
        InitLogger();
        Log(L"Application start");
        Log(L"wWinMain: entered");
        
        // Set unhandled exception filter using a regular function
        SetUnhandledExceptionFilter(AppUnhandledExceptionFilter);
                
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::vector<std::wstring> args;
        if (argv != nullptr) {
            for (int i = 1; i < argc; ++i) {
                args.push_back(argv[i]);
            }
            LocalFree(argv);
        }

        ArgumentDebuggerWindow debugger_app;
        g_app_instance = &debugger_app;

        debugger_app.Initialize(h_instance, cmd_show, args);

        int exit_code = debugger_app.RunMessageLoop();
        g_app_instance = nullptr;
        
        Log(L"Application exit, code = " + std::to_wstring(exit_code));
        Log(L"wWinMain: leaving, exitCode=" + std::to_wstring(exit_code));
        
        // Close the log file and delete critical section
        if (g_log_file) {
            fclose(g_log_file);
            g_log_file = nullptr;
        }
        
        // Delete critical section
        DeleteCriticalSection(&g_log_cs);
        
        CoUninitialize();
        return exit_code;
    }
    catch (const std::exception &ex) {
        // Convert char* to wstring for logging
        std::wstring wstr(ex.what(), ex.what() + strlen(ex.what()));
        Log(L"Unhandled C++ exception");
        Log(L"FATAL: " + wstr);
        
        // Close log file before exit
        if (g_log_file) {
            fclose(g_log_file);
            g_log_file = nullptr;
        }
        
        // Delete critical section in exception case as well
        DeleteCriticalSection(&g_log_cs);
        
        MessageBoxA(nullptr, ex.what(), "Initialization Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}

void ArgumentDebuggerWindow::Initialize(HINSTANCE h_instance, int cmd_show, const std::vector<std::wstring>& args) {
    args_ = args;
    InitializeWindow(h_instance, cmd_show);
    InitializeDevice();
}

int ArgumentDebuggerWindow::RunMessageLoop() {
    Log(L"RunMessageLoop: started");
    MSG msg = {};
    last_time_ = GetTickCount64();

    while (is_running_) {
        bool hadMsg = false;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            hadMsg = true;
            if (msg.message == WM_QUIT) {
                Log(L"WM_QUIT received, wParam=" + std::to_wstring(msg.wParam));
                is_running_ = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        if (!hadMsg) {
            // Log only once every 10 seconds to avoid bloating the log file
            static ULONGLONG t0 = GetTickCount64();
            ULONGLONG now = GetTickCount64();
            if (now - t0 > 10000) {
                Log(L"RunMessageLoop: idle");
                t0 = now;
            }
        }
        
        try {
            RenderFrame();
        } catch (const std::exception& ex) {
            Log(L"Render error: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
            MessageBoxA(window_handle_, ex.what(), "Render error", MB_ICONERROR);
            PostQuitMessage(1);
            break;
        }
    }
    
    Log(L"RunMessageLoop: finished");
    return static_cast<int>(msg.wParam);
}

// Keyboard input handling: added support for "save", "read" and "logs" commands
void ArgumentDebuggerWindow::OnCharInput(wchar_t ch) {
    if (ch == VK_RETURN) {
        if (_wcsicmp(user_input_.c_str(), L"exit") == 0) {
            Log(L"Command: exit");
            PostQuitMessage(0);
            is_running_ = false;
        }
        else if (_wcsicmp(user_input_.c_str(), L"save") == 0) {
            Log(L"Command: save");
            SaveData();
        }
        else if (_wcsicmp(user_input_.c_str(), L"read") == 0) {
            Log(L"Command: read");
            ReadData();
        }
        else if (_wcsicmp(user_input_.c_str(), L"logs") == 0) {
            Log(L"Command: logs");
            ShowLogs();
        }
        else {
            command_status_ = L"Unknown command.";
        }
        user_input_.clear();
    }
    else if (ch == VK_BACK) {
        if (!user_input_.empty())
            user_input_.pop_back();
    }
    else if (ch == VK_ESCAPE) {
        Log(L"Command: exit (via Escape)");
        PostQuitMessage(0);
        is_running_ = false;
    }
    else {
        user_input_ += ch;
    }
}

void ArgumentDebuggerWindow::OnDestroy() {
    Log(L"Window destroy event");
    
    // 1. Signal threads to exit via atomic flags
    is_running_ = false;
    audio_thread_running_.store(false);
    
    // Wake up audio thread if waiting
    if (audio_event_) SetEvent(audio_event_);
    
    // 2. Wait for thread completion
    if (audio_thread_) {
        WaitForSingleObject(audio_thread_, INFINITE);
        CloseHandle(audio_thread_);
        audio_thread_ = nullptr;
    }
    if (audio_event_) {                     // Close event handle
        CloseHandle(audio_event_);
        audio_event_ = nullptr;
    }
    
    Cleanup();                              // 3. Now safely release COM objects
    PostQuitMessage(0);
}

void ArgumentDebuggerWindow::InitializeWindow(HINSTANCE h_instance, int /*cmd_show*/) {
    Log(L"InitializeWindow: registering class");
    // Register and create the window.
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = h_instance;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClass(&wc))
        throw std::runtime_error("Failed to register window class.");

    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    window_handle_ = CreateWindowEx(
        WS_EX_TOPMOST,
        kWindowClassName,
        kWindowCaption,
        WS_POPUP,
        0, 0, screen_width, screen_height,
        nullptr, nullptr, h_instance, nullptr);

    if (!window_handle_)
        throw std::runtime_error("Failed to create window.");
        
    Log(L"InitializeWindow: HWND=" + std::to_wstring(reinterpret_cast<uintptr_t>(window_handle_)));

    ShowWindow(window_handle_, SW_SHOWMAXIMIZED);
}

void ArgumentDebuggerWindow::InitializeDevice() {
    RECT rc;
    GetClientRect(window_handle_, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    CreateDeviceAndSwapChain(width, height);
    CreateRenderTargetView();
    CreateD2DResources();
    CreateShadersAndGeometry();
    InitializeMicrophone();
}

void ArgumentDebuggerWindow::CreateDeviceAndSwapChain(UINT width, UINT height) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHARED;
    sd.OutputWindow = window_handle_;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // Enable debug layer for better diagnostics in debug builds
#ifdef _DEBUG
    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

    DX_CALL(
        D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_flags,
            nullptr, 0, D3D11_SDK_VERSION,
            &sd, swap_chain_.GetAddressOf(),
            d3d_device_.GetAddressOf(), nullptr, immediate_context_.GetAddressOf()
        ),
        "Failed to create Direct3D device and swap chain."
    );
}

void ArgumentDebuggerWindow::CreateRenderTargetView() {
    ComPtr<ID3D11Texture2D> back_buffer;
    DX_CALL(
        swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back_buffer.GetAddressOf())),
        "Failed to get back buffer."
    );
    DX_CALL(
        d3d_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, d3d_render_target_view_.GetAddressOf()),
        "Failed to create render target view."
    );
}

void ArgumentDebuggerWindow::CreateD2DResources() {
    // Create the Direct2D factory.
    DX_CALL(
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, D2D1_FACTORY_OPTIONS{}, d2d_factory_.GetAddressOf()),
        "Failed to create Direct2D factory."
    );

    // Get the DXGI surface from the swap chain.
    ComPtr<IDXGISurface> dxgi_surface;
    DX_CALL(
        swap_chain_->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface)),
        "Failed to get DXGI surface."
    );

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0, 0
    );
    DX_CALL(
        d2d_factory_->CreateDxgiSurfaceRenderTarget(dxgi_surface.Get(), &props, d2d_render_target_.GetAddressOf()),
        "Failed to create Direct2D render target."
    );

    // Create the DirectWrite factory and text format.
    DX_CALL(
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf())),
        "Failed to create DirectWrite factory."
    );
    DX_CALL(
        dwrite_factory_->CreateTextFormat(
            L"Arial", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            24.0f,
            L"en-us",
            text_format_.GetAddressOf()
        ),
        "Failed to create text format."
    );
    text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    
    // Create a smaller text format for logs (half the size of the regular text)
    DX_CALL(
        dwrite_factory_->CreateTextFormat(
            L"Consolas", nullptr,  // Using monospaced font for logs
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,  // Half the size of the regular font
            L"en-us",
            small_text_format_.GetAddressOf()
        ),
        "Failed to create small text format."
    );
    // Set text alignment properties
    small_text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    small_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    // Use trailing alignment for device name text to align to the right
    small_text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void ArgumentDebuggerWindow::CreateShadersAndGeometry() {
    // Vertex shader source code.
    constexpr const char vs_src[] =
        "cbuffer ConstantBuffer : register(b0) {"
        "    matrix WorldViewProjection;"
        "};"
        "struct VS_INPUT { float3 Pos : POSITION; float3 Color : COLOR; };"
        "struct PS_INPUT { float4 Pos : SV_POSITION; float3 Color : COLOR; };"
        "PS_INPUT VSMain(VS_INPUT input) {"
        "    PS_INPUT output;"
        "    output.Pos = mul(float4(input.Pos, 1.0f), WorldViewProjection);"
        "    output.Color = input.Color;"
        "    return output;"
        "}";

    // Pixel shader source code.
    constexpr const char ps_src[] =
        "struct PS_INPUT { float4 Pos : SV_POSITION; float3 Color : COLOR; };"
        "float4 PSMain(PS_INPUT input) : SV_Target {"
        "    return float4(input.Color, 1.0f);"
        "}";

    ComPtr<ID3DBlob> vs_blob, ps_blob, error_blob;
    DX_CALL(
        D3DCompile(vs_src, sizeof(vs_src) - 1, nullptr, nullptr, nullptr,
                   "VSMain", "vs_4_0", 0, 0, vs_blob.GetAddressOf(), error_blob.GetAddressOf()),
        "Failed to compile vertex shader."
    );
    DX_CALL(
        D3DCompile(ps_src, sizeof(ps_src) - 1, nullptr, nullptr, nullptr,
                   "PSMain", "ps_4_0", 0, 0, ps_blob.GetAddressOf(), error_blob.GetAddressOf()),
        "Failed to compile pixel shader."
    );

    DX_CALL(
        d3d_device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, vertex_shader_.GetAddressOf()),
        "Failed to create vertex shader."
    );
    DX_CALL(
        d3d_device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, pixel_shader_.GetAddressOf()),
        "Failed to create pixel shader."
    );

    // Define input layout.
    std::array<D3D11_INPUT_ELEMENT_DESC, 2> layout_desc = {{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,   D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,  D3D11_INPUT_PER_VERTEX_DATA, 0}
    }};
    DX_CALL(
        d3d_device_->CreateInputLayout(layout_desc.data(), static_cast<UINT>(layout_desc.size()),
                                       vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                       vertex_layout_.GetAddressOf()),
        "Failed to create input layout."
    );
    immediate_context_->IASetInputLayout(vertex_layout_.Get());

    // Define vertices (cube).
    std::array<SimpleVertex, 8> vertices = {{
        // Front face
        {DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f)},
        {DirectX::XMFLOAT3( 1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)},
        {DirectX::XMFLOAT3( 1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f)},
        {DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f)},
        // Back face
        {DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f)},
        {DirectX::XMFLOAT3( 1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f)},
        {DirectX::XMFLOAT3( 1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)},
        {DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)}
    }};

    // Create vertex buffer.
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = static_cast<UINT>(sizeof(SimpleVertex) * vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = vertices.data();
    DX_CALL(
        d3d_device_->CreateBuffer(&bd, &init_data, vertex_buffer_.GetAddressOf()),
        "Failed to create vertex buffer."
    );

    // Define indices.
    std::array<WORD, 36> indices = {{
        0,1,2, 2,3,0,
        4,7,6, 6,5,4,
        4,0,3, 3,7,4,
        1,5,6, 6,2,1,
        4,5,1, 1,0,4,
        3,2,6, 6,7,3
    }};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = static_cast<UINT>(sizeof(WORD) * indices.size());
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    init_data.pSysMem = indices.data();
    DX_CALL(
        d3d_device_->CreateBuffer(&bd, &init_data, index_buffer_.GetAddressOf()),
        "Failed to create index buffer."
    );

    // Create constant buffer.
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBufferData);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    DX_CALL(
        d3d_device_->CreateBuffer(&bd, nullptr, constant_buffer_.GetAddressOf()),
        "Failed to create constant buffer."
    );
}

void ArgumentDebuggerWindow::UpdateQrCode(ULONGLONG current_time) {
    // Update the QR code no more than once every 5 seconds.
    if (current_time - last_qr_update_time_ < 5000)
        return;
    last_qr_update_time_ = current_time;

    time_t unix_time = time(nullptr);
    // Get the integer value of FPS.
    int fps_for_qr = static_cast<int>(current_fps_);

    // Save this value for synchronized file output.
    synced_fps_ = fps_for_qr;

    // Create the string for the QR code
    std::ostringstream oss;
    oss << "t=" << unix_time << ";f=" << fps_for_qr;
    if (!args_.empty()) {
        oss << ";args=";
        for (const auto& arg : args_) {
            oss << wstring_to_string(arg) << " ";
        }
    }
    std::string qr_data = oss.str();

    // Generate the QR code (error correction level MEDIUM).
    QrCode qr = QrCode::encodeText(qr_data.c_str(), QrCode::Ecc::MEDIUM);
    int qr_modules = qr.getSize();
    constexpr int pixel_size = 375;  // final size 375x375
    float scale = static_cast<float>(pixel_size) / qr_modules;
    std::vector<uint32_t> pixels(pixel_size * pixel_size, 0xffffffff); // white background

    for (int y = 0; y < pixel_size; y++) {
        for (int x = 0; x < pixel_size; x++) {
            int module_x = static_cast<int>(x / scale);
            int module_y = static_cast<int>(y / scale);
            if (module_x < qr_modules && module_y < qr_modules) {
                if (qr.getModule(module_x, module_y))
                    pixels[y * pixel_size + x] = 0xff000000; // black pixel
            }
        }
    }

    D2D1_BITMAP_PROPERTIES bitmapProperties = {};
    bitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    bitmapProperties.dpiX = 96.0f;
    bitmapProperties.dpiY = 96.0f;

    qr_bitmap_.Reset();
    DX_CALL(
        d2d_render_target_->CreateBitmap(
            D2D1::SizeU(pixel_size, pixel_size),
            pixels.data(),
            pixel_size * sizeof(uint32_t),
            &bitmapProperties,
            qr_bitmap_.GetAddressOf()
        ),
        "Failed to create QR code bitmap."
    );
}

void ArgumentDebuggerWindow::RenderFrame() {
    ULONGLONG current_time = GetTickCount64();
    float delta_time = (current_time - last_time_) / 1000.0f;
    last_time_ = current_time;

    // Update the instantaneous FPS
    current_fps_ = (delta_time > 0.0f) ? (1.0f / delta_time) : 0.0f;

    UpdateRotation(delta_time);

    // Clear the D3D render target
    FLOAT clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    immediate_context_->ClearRenderTargetView(d3d_render_target_view_.Get(), clear_color);
    immediate_context_->OMSetRenderTargets(1, d3d_render_target_view_.GetAddressOf(), nullptr);

    RECT rc;
    GetClientRect(window_handle_, &rc);
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(rc.right - rc.left);
    vp.Height = static_cast<float>(rc.bottom - rc.top);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    immediate_context_->RSSetViewports(1, &vp);

    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    immediate_context_->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);
    immediate_context_->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
    immediate_context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    using namespace DirectX;
    XMMATRIX world = XMMatrixRotationY(rotation_angle_);
    XMVECTOR eye = XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f);
    XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, vp.Width / vp.Height, 0.01f, 100.0f);

    ConstantBufferData cb_data;
    cb_data.world_view_projection = XMMatrixTranspose(world * view * proj);
    immediate_context_->UpdateSubresource(constant_buffer_.Get(), 0, nullptr, &cb_data, 0, 0);

    immediate_context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
    immediate_context_->VSSetConstantBuffers(0, 1, constant_buffer_.GetAddressOf());
    immediate_context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);

    immediate_context_->DrawIndexed(36, 0, 0);

    // Begin Direct2D drawing.
    d2d_render_target_->BeginDraw();

    // Update the QR code (no more than once every 5 seconds).
    UpdateQrCode(current_time);

    // Create brushes.
    ComPtr<ID2D1SolidColorBrush> white_brush, green_brush, yellow_brush;
    DX_CALL(
        d2d_render_target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), white_brush.GetAddressOf()),
        "Failed to create white brush."
    );
    DX_CALL(
        d2d_render_target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green), green_brush.GetAddressOf()),
        "Failed to create green brush."
    );
    DX_CALL(
        d2d_render_target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), yellow_brush.GetAddressOf()),
        "Failed to create yellow brush."
    );

    D2D1_SIZE_F size = d2d_render_target_->GetSize();
    float y_pos = kMargin;

    // Draw description lines.
    for (const auto& line : kDescriptionLines) {
        D2D1_RECT_F rect = D2D1::RectF(kMargin, y_pos, size.width - kMargin, y_pos + kLineHeight);
        d2d_render_target_->DrawText(line.c_str(), static_cast<UINT32>(line.size()),
                                      text_format_.Get(), rect, white_brush.Get());
        y_pos += kLineHeight;
    }
    y_pos += kLineHeight;

    // Draw command-line argument header.
    std::wstring cli_text = BuildCliHeaderText(args_);
    {
        D2D1_RECT_F rect = D2D1::RectF(kMargin, y_pos, size.width - kMargin, y_pos + kLineHeight);
        d2d_render_target_->DrawText(cli_text.c_str(), static_cast<UINT32>(cli_text.size()),
                                      text_format_.Get(), rect, green_brush.Get());
    }
    y_pos += kLineHeight;

    // Draw the actual command-line arguments (if any).
    if (!args_.empty()) {
        std::wstring formatted_args = BuildCliArgsText(args_);
        D2D1_RECT_F args_rect = D2D1::RectF(kMargin, y_pos, size.width - kMargin, size.height - 200.0f);
        d2d_render_target_->DrawText(formatted_args.c_str(), static_cast<UINT32>(formatted_args.size()),
                                      text_format_.Get(), args_rect, green_brush.Get());
        y_pos += kLineHeight;
    }

    y_pos += 10.0f;
    // Additional drawing: display command execution status.
    {
        D2D1_RECT_F status_rect = D2D1::RectF(kMargin, size.height - 220.0f, size.width - kMargin, size.height - 190.0f);
        d2d_render_target_->DrawText(command_status_.c_str(), static_cast<UINT32>(command_status_.size()),
                                      text_format_.Get(), status_rect, white_brush.Get());
    }

    // Draw loaded data (if any) in the top-right corner.
    if (!loaded_data_.empty()) {
        // Use a smaller font for logs and expand the display area
        // Added padding at the bottom (reduced height by 20px)
        D2D1_RECT_F data_rect = D2D1::RectF(size.width - 750.0f, kMargin, size.width - kMargin, kMargin + 380.0f);
        d2d_render_target_->DrawText(loaded_data_.c_str(), static_cast<UINT32>(loaded_data_.size()),
                                      small_text_format_.Get(), data_rect, green_brush.Get());
        
        // Add a title for the logs section
        D2D1_RECT_F title_rect = D2D1::RectF(size.width - 750.0f, kMargin - 30.0f, size.width - kMargin, kMargin);
        d2d_render_target_->DrawText(L"Log File Contents:", 17,
                                     text_format_.Get(), title_rect, yellow_brush.Get());
    }

    // Display full and relative path where the application is running
    wchar_t exePath[MAX_PATH] = L"\0";
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring fullPath = exePath;
    
    // Get current working directory for relative path
    wchar_t currentDir[MAX_PATH] = L"\0";
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    std::wstring currentDirStr = currentDir;
    
    // Extract just the executable name
    std::wstring exeName = fullPath;
    size_t lastSlash = exeName.find_last_of(L"\\");
    if (lastSlash != std::wstring::npos) {
        exeName = exeName.substr(lastSlash + 1);
    }
    
    // Extract directory from full path
    std::wstring exeDir = fullPath;
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    
    // Calculate relative path
    std::wstring relativePath;
    if (fullPath.find(currentDirStr) == 0) {
        // If the exe is within the current directory, show relative path
        relativePath = L"." + fullPath.substr(currentDirStr.length());
    } else {
        // Otherwise, just show the full path
        relativePath = fullPath;
    }
    
    // Get TEMP directory
    wchar_t tempPath[MAX_PATH] = L"\0";
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring tempDir = tempPath;
    if (!tempDir.empty() && tempDir.back() == L'\\') {
        tempDir.pop_back(); // Remove trailing slash
    }
    
    // Get Windows directory
    wchar_t winPath[MAX_PATH] = L"\0";
    GetWindowsDirectoryW(winPath, MAX_PATH);
    std::wstring winDir = winPath;
    
    // Get System directory
    wchar_t sysPath[MAX_PATH] = L"\0";
    GetSystemDirectoryW(sysPath, MAX_PATH);
    std::wstring sysDir = sysPath;
    
    // Get command line
    std::wstring cmdLine = GetCommandLineW();
    
    // Use smaller font for path information
    float pathStartY = size.height - 280.0f;
    float pathLineHeight = 25.0f;
    
    // Draw all path information with smaller font
    std::vector<std::pair<std::wstring, std::wstring>> pathItems = {
        {L"Executable name: ", exeName},
        {L"Full path: ", fullPath},
        {L"Executable directory: ", exeDir},
        {L"Current directory: ", currentDirStr},
        {L"Command line: ", cmdLine},
        {L"TEMP directory: ", tempDir},
        {L"Windows directory: ", winDir},
        {L"System directory: ", sysDir}
    };
    
    float currentY = pathStartY;
    for (const auto& item : pathItems) {
        std::wstring fullLine = item.first + item.second;
        D2D1_RECT_F rect = D2D1::RectF(kMargin, currentY, size.width - kMargin, currentY + pathLineHeight);
        d2d_render_target_->DrawText(fullLine.c_str(), static_cast<UINT32>(fullLine.size()),
                                      small_text_format_.Get(), rect, white_brush.Get());
        currentY += pathLineHeight;
    }

    // Input field and prompt.
    std::wstring exit_prompt = L"Type 'exit', 'save', 'read' or 'logs' and press Enter:";
    D2D1_RECT_F exit_prompt_rect = D2D1::RectF(kMargin, size.height - 100.0f, size.width - kMargin, size.height - 70.0f);
    d2d_render_target_->DrawText(exit_prompt.c_str(), static_cast<UINT32>(exit_prompt.size()),
                                  text_format_.Get(), exit_prompt_rect, yellow_brush.Get());

    D2D1_RECT_F user_input_rect = D2D1::RectF(kMargin, size.height - 60.0f, size.width - kMargin, size.height - 30.0f);
    d2d_render_target_->DrawText(user_input_.c_str(), static_cast<UINT32>(user_input_.size()),
                                  text_format_.Get(), user_input_rect, green_brush.Get());

    // Draw the QR code in the bottom-left corner.
    if (qr_bitmap_) {
        constexpr int qr_size = 375;
        constexpr float qr_margin = 60.0f;
        float qr_x = qr_margin;
        float qr_y = size.height - qr_size - qr_margin - 100.0f;
        d2d_render_target_->DrawBitmap(qr_bitmap_.Get(), D2D1::RectF(qr_x, qr_y, qr_x + qr_size, qr_y + qr_size));
    }

    // --- Volume Meter (L/R) ---
    if (mic_available_) {
        float level = mic_level_.load();           // 0..1
        
        // Draw two volume bars - one for left, one for right
        float bar_w = 30.0f, bar_h = 150.0f;
        float spacing = 15.0f;
        float total_width = bar_w * 2 + spacing;
        float x0 = size.width - kMargin - total_width;
        float y0 = size.height - kMargin - bar_h;
        
        // Device name title with improved layout
        std::wstring dev_title = L"Mic: " + (mic_name_.empty() ? L"<unknown>" : mic_name_);
        
        // Position device name at the right edge of the screen
        // parameters for the text area
        float devAreaWidth = 200.0f;              // width of the area
        float devAreaHeight = 2 * kLineHeight;     // two lines high
        float marginRight = kMargin;              // margin from right edge of screen
        float marginBottom = 5.0f;                // margin above bars

        // right edge of the area - just after the screen margin
        float devRight = size.width - marginRight;
        // left - right minus area width
        float devLeft = devRight - devAreaWidth;
        // bottom of area - y0 (top of the bars)
        // y0 is already defined: y0 = size.height - kMargin - bar_h
        // top - bottom minus area height and small margin
        float devBottom = y0;
        float devTop = devBottom - devAreaHeight - marginBottom;

        D2D1_RECT_F dev_rect = D2D1::RectF(
            devLeft, devTop,
            devRight, devBottom);
            
        // Draw multiline text
        d2d_render_target_->DrawText(dev_title.c_str(), (UINT32)dev_title.size(),
                                     small_text_format_.Get(), dev_rect, white_brush.Get());
        
        // Left channel (using the same level for both channels in this demo)
        // In a real stereo implementation, you'd capture separate L/R levels
        float left_level = level;
        float left_filled = bar_h * left_level;
        
        // Left bar outline
        d2d_render_target_->DrawRectangle(
            D2D1::RectF(x0, y0, x0 + bar_w, y0 + bar_h),
            white_brush.Get(), 2.0f);
        
        // Left bar fill
        d2d_render_target_->FillRectangle(
            D2D1::RectF(x0, y0 + (bar_h - left_filled), x0 + bar_w, y0 + bar_h),
            green_brush.Get());
        
        // Right channel (using same level for demo, but with slight variation)
        float right_level = level * 0.9f; // Slight variation for demo
        float right_filled = bar_h * right_level;
        float right_x = x0 + bar_w + spacing;
        
        // Right bar outline
        d2d_render_target_->DrawRectangle(
            D2D1::RectF(right_x, y0, right_x + bar_w, y0 + bar_h),
            white_brush.Get(), 2.0f);
        
        // Right bar fill
        d2d_render_target_->FillRectangle(
            D2D1::RectF(right_x, y0 + (bar_h - right_filled), right_x + bar_w, y0 + bar_h),
            green_brush.Get());
        
        // Draw L/R labels
        std::wstring left_label = L"L";
        std::wstring right_label = L"R";
        
        D2D1_RECT_F left_label_rect = D2D1::RectF(x0, y0 - 30.0f, x0 + bar_w, y0);
        D2D1_RECT_F right_label_rect = D2D1::RectF(right_x, y0 - 30.0f, right_x + bar_w, y0);
        
        d2d_render_target_->DrawText(left_label.c_str(), (UINT32)left_label.size(),
                                     text_format_.Get(), left_label_rect, yellow_brush.Get());
        d2d_render_target_->DrawText(right_label.c_str(), (UINT32)right_label.size(),
                                     text_format_.Get(), right_label_rect, yellow_brush.Get());
    } else {
        std::wstring no_mic = L"No microphone detected";
        D2D1_RECT_F r = D2D1::RectF(size.width - 300.f, size.height - 50.f,
                                    size.width - kMargin, size.height - kMargin);
        d2d_render_target_->DrawText(no_mic.c_str(), (UINT32)no_mic.size(),
                                     text_format_.Get(), r, yellow_brush.Get());
    }

    // End Direct2D drawing with proper error handling
    HRESULT hrEnd = d2d_render_target_->EndDraw();
    if (hrEnd == D2DERR_RECREATE_TARGET) { 
        // Device lost, resources need to be recreated
        Log(L"Device lost detected, recreating D2D resources");
        CreateD2DResources(); 
        return; 
    }
    if (FAILED(hrEnd))
        throw std::runtime_error("Failed to end Direct2D draw.");

    // Log FPS no more than once every 5 seconds to avoid cluttering the log
    static ULONGLONG lastFpsLogTime = 0;
    ULONGLONG currentTime = GetTickCount64();
    if (currentTime - lastFpsLogTime > 5000) {
        Log(L"RenderFrame: Present FPS=" + std::to_wstring(static_cast<int>(current_fps_)));
        lastFpsLogTime = currentTime;
    }
    
    // Present the frame on screen with error checking
    HRESULT hrPresent = swap_chain_->Present(1, 0);
    if (hrPresent == DXGI_ERROR_DEVICE_REMOVED || hrPresent == DXGI_ERROR_DEVICE_RESET) {
        // Device was removed or reset, recreate resources
        Log(L"Device removed/reset detected, recreating all graphics resources");
        CreateDeviceAndSwapChain(
            static_cast<UINT>(d2d_render_target_->GetSize().width),
            static_cast<UINT>(d2d_render_target_->GetSize().height)
        );
        CreateRenderTargetView();
        CreateD2DResources();
        CreateShadersAndGeometry();
        return;
    }
    if (FAILED(hrPresent))
        throw std::runtime_error("Failed to present frame.");
}

void ArgumentDebuggerWindow::UpdateRotation(float delta_time) {
    rotation_angle_ += delta_time * DirectX::XM_PIDIV4 / 2.0f;
}

// SEH wrapper function is implemented in seh_wrapper.cpp

// Entry point that forwards to the raw SEH function
__declspec(nothrow) DWORD WINAPI ArgumentDebuggerWindow::AudioCaptureThread(LPVOID param) {
    // This is just a thin wrapper to maintain the class method interface
    return RawAudioThreadWithSEH(param);
}

// The actual implementation with C++ exception handling
DWORD ArgumentDebuggerWindow::AudioCaptureThreadImpl(LPVOID param) {
    // Each thread needs its own COM initialization
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return 0;
    
    auto self = static_cast<ArgumentDebuggerWindow*>(param);
    HANDLE mmHandle = nullptr; // Multimedia handle for thread priority
    
    try {
        // Set audio thread priorities
        DWORD taskIndex = 0;
        mmHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        if (!mmHandle) {
            Log(L"Warning: AvSetMmThreadCharacteristicsW failed");
        }
        self->audio_client_->Start();
        
        Log(L"Audio capture thread started");
        
        while (self->audio_thread_running_.load()) {
            DWORD waitResult = WaitForSingleObject(self->audio_event_, 200);
            if (waitResult == WAIT_OBJECT_0) {
                Log(L"Audio thread: signal received");
            } else if (waitResult == WAIT_TIMEOUT) {
                // Log timeouts only once per 30 seconds to avoid log spam
                static ULONGLONG lastTimeoutLog = 0;
                ULONGLONG now = GetTickCount64();
                if (now - lastTimeoutLog > 30000) {
                    Log(L"Audio thread: timeout (normal)");
                    lastTimeoutLog = now;
                }
            } else {
                Log(L"Audio thread: wait failed, code=" + std::to_wstring(waitResult));
            }
            
            // Call PollMicrophone directly - SEH handling moved to AudioCaptureThread
            self->PollMicrophone();
        }
        self->audio_client_->Stop();
        
        Log(L"Audio capture thread stopped");
    }
    catch (const std::exception& ex) {
        // Log exception from audio thread
        Log(L"Audio thread exception: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
    }
    catch (...) {
        // Log unknown exception from audio thread
        Log(L"Audio thread: unknown exception");
    }
    
    // Revert thread characteristics if we set them
    if (mmHandle) {
        AvRevertMmThreadCharacteristics(mmHandle);
    }
    
    CoUninitialize();
    return 0;
}

void ArgumentDebuggerWindow::InitializeMicrophone() {
    // No COM initialization here - it's already done in wWinMain

    try {
        DX_CALL( CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_ALL, IID_PPV_ARGS(&device_enumerator_)),
                "IMMDeviceEnumerator failed" );

        DX_CALL( device_enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole,
                                                            capture_device_.GetAddressOf()),
                "No default capture device" );

        // --- FriendlyName -----------------------------------------------------------------
        ComPtr<IPropertyStore> store;
        DX_CALL(capture_device_->OpenPropertyStore(STGM_READ, &store),
                "OpenPropertyStore failed");

        PROPVARIANT pv;
        PropVariantInit(&pv);
        DX_CALL(store->GetValue(PKEY_Device_FriendlyName, &pv),
                "GetValue(FriendlyName) failed");

        mic_name_ = pv.vt == VT_LPWSTR ? pv.pwszVal : L"Unknown microphone";
        PropVariantClear(&pv);
        // ----------------------------------------------------------------------------------

        mic_available_ = true;

        DX_CALL( capture_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                        reinterpret_cast<void**>(audio_client_.GetAddressOf())),
                "IAudioClient activate failed" );

        DX_CALL( audio_client_->GetMixFormat(&mix_format_), "GetMixFormat failed" );

        // Increased buffer size to 100ms for smoother visualization
        REFERENCE_TIME buf_dur = 100 * 10000; // 1ms = 10,000 * 100ns
        DX_CALL( audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                        buf_dur, 0, mix_format_, nullptr),
                "AudioClient init failed" );

        DX_CALL( audio_client_->GetService(IID_PPV_ARGS(&capture_client_)),
                "GetService(IAudioCaptureClient)" );

        audio_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (audio_event_ == nullptr) {
            throw std::runtime_error("Failed to create audio event");
        }
        audio_client_->SetEventHandle(audio_event_);

        // Set running flag before starting the thread
        audio_thread_running_.store(true);
        
        // Audio capture thread: use SEH-wrapped version
        audio_thread_ = CreateThread(nullptr, 0,
                        ArgumentDebuggerWindow::AudioCaptureThread,
                        this, 0, nullptr);
                        
        Log(L"Microphone initialized successfully");
    }
    catch (const std::exception& ex) {
        Log(L"Microphone initialization failed: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
        mic_available_ = false;    // Will display "No microphone detected"
        return;                    // Don't create thread if initialization failed
    }
}

void ArgumentDebuggerWindow::PollMicrophone() {
    try {
        // Quick check to bail out if thread should terminate
        if (!audio_thread_running_.load()) {
            return;
        }
        
        // Safety check for capture_client_ pointer
        if (!capture_client_) {
            Log(L"PollMicrophone: capture_client_ is NULL");
            return;
        }
        
        UINT32 pkt_len = 0;
        HRESULT hr = capture_client_->GetNextPacketSize(&pkt_len);
        if (FAILED(hr)) {
            Log(L"PollMicrophone: Initial GetNextPacketSize failed, hr=0x" + std::to_wstring(hr));
            return;
        }
        
        // No packets available
        if (pkt_len == 0) {
            return;
        }

        while (pkt_len > 0) {
            BYTE*  data   = nullptr;
            UINT32 frames = 0;
            DWORD  flags  = 0;
            hr = capture_client_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            
            if (FAILED(hr)) {
                Log(L"PollMicrophone: GetBuffer failed, hr=0x" + std::to_wstring(hr));
                break;
            }
            
            // Bail out if buffer is invalid
            if (data == nullptr || frames == 0) {
                std::wstring dataStatus = data ? L"valid" : L"NULL";
                Log(L"PollMicrophone: Invalid buffer - data=" + dataStatus + 
                    L", frames=" + std::to_wstring(frames));
                    
                // Still need to release the buffer even if data is NULL
                if (SUCCEEDED(hr)) {
                    capture_client_->ReleaseBuffer(frames);
                }
                break;
            }
            
            // Log buffer details
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                Log(L"PollMicrophone: Silent buffer detected, frames=" + std::to_wstring(frames) + 
                    L", flags=0x" + std::to_wstring(flags));
            } else {
                Log(L"PollMicrophone: Buffer received: frames=" + std::to_wstring(frames) + 
                    L", flags=0x" + std::to_wstring(flags));
            }

            // For mono/stereo, 16-bit/32-bit float - taking absolute max value
            float peak = 0.f;
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data != nullptr && mix_format_ != nullptr) {
                WORD tag = mix_format_->wFormatTag;
                WORD bps = mix_format_->wBitsPerSample;

                if (tag == WAVE_FORMAT_EXTENSIBLE) {
                    // Safe cast to WAVEFORMATEXTENSIBLE* but verify size first
                    if (mix_format_->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
                        auto wfex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format_);
                        
                        // Safely check GUID comparison
                        if (IsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
                            tag = WAVE_FORMAT_IEEE_FLOAT;
                        else if (IsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
                            tag = WAVE_FORMAT_PCM;
                        else {
                            Log(L"Unknown SubFormat GUID in WAVE_FORMAT_EXTENSIBLE");
                        }
                        
                        // Use wValidBitsPerSample if non-zero, otherwise use format's bit depth
                        bps = wfex->Samples.wValidBitsPerSample;
                        if (bps == 0) {
                            bps = wfex->Format.wBitsPerSample;
                        }
                    } else {
                        Log(L"WAVE_FORMAT_EXTENSIBLE with invalid cbSize: " + std::to_wstring(mix_format_->cbSize));
                        // Fall back to basic format info
                        tag = WAVE_FORMAT_PCM; // Assume PCM as fallback
                        bps = mix_format_->wBitsPerSample;
                    }
                }

                // Safely calculate total samples, checking for integer overflow
                UINT32 channels = mix_format_->nChannels;
                if (channels == 0) {
                    Log(L"Invalid channel count: 0");
                    channels = 1; // Default to mono
                }
                
                // Check for integer overflow
                UINT64 totalSamples64 = static_cast<UINT64>(frames) * static_cast<UINT64>(channels);
                if (totalSamples64 > UINT32_MAX) {
                    Log(L"Sample count overflow: " + std::to_wstring(totalSamples64));
                    break;
                }
                
                UINT32 totalSamples = static_cast<UINT32>(totalSamples64);
                
                try {
                    if (tag == WAVE_FORMAT_IEEE_FLOAT && bps == 32) {
                        const float* samples = reinterpret_cast<const float*>(data);
                        for (UINT32 i = 0; i < totalSamples; ++i) {
                            float val = samples[i];
                            if (val < 0) val = -val;
                            if (val > peak) peak = val;
                        }
                    } 
                    else if (tag == WAVE_FORMAT_PCM && bps == 16) {
                        const int16_t* samples = reinterpret_cast<const int16_t*>(data);
                        for (UINT32 i = 0; i < totalSamples; ++i) {
                            float val = samples[i] / 32768.0f;
                            if (val < 0) val = -val;
                            if (val > peak) peak = val;
                        }
                    }
                    else if (tag == WAVE_FORMAT_PCM && bps == 24) {
                        // 24-bit PCM is stored as 3 bytes per sample
                        const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
                        
                        // Check if we have enough bytes for all samples (3 bytes per sample)
                        UINT64 byteCount = static_cast<UINT64>(totalSamples) * 3;
                        if (byteCount > SIZE_MAX) {
                            Log(L"Byte count overflow for 24-bit audio");
                            break;
                        }
                        
                        for (UINT32 i = 0; i < totalSamples; ++i) {
                            // Load 3 bytes into 32-bit integer (sign-extended)
                            int32_t sample = (p[3*i] << 8) | (p[3*i+1] << 16) | (p[3*i+2] << 24);
                            sample >>= 8; // Shift back to get correct sign extension
                            
                            // Convert to float -1.0 to 1.0 (normalize by 2^23)
                            float val = sample / 8388608.0f;
                            if (val < 0) val = -val;
                            if (val > peak) peak = val;
                        }
                    }
                    else if (tag == WAVE_FORMAT_PCM && bps == 32) {
                        const int32_t* samples = reinterpret_cast<const int32_t*>(data);
                        for (UINT32 i = 0; i < totalSamples; ++i) {
                            // Normalize by 2^31
                            float val = samples[i] / 2147483648.0f;
                            if (val < 0) val = -val;
                            if (val > peak) peak = val;
                        }
                    }
                    else {
                        // Unsupported format
                        Log(L"Unsupported audio format: tag=" + std::to_wstring(tag) + 
                            L", bps=" + std::to_wstring(bps));
                    }
                }
                catch (...) {
                    Log(L"Exception during audio processing");
                }
                
                // Temporary logging to verify data is coming in
                Log(L"PollMicrophone: peak=" + std::to_wstring(peak) + 
                    L", format tag=" + std::to_wstring(tag) + 
                    L", bps=" + std::to_wstring(bps));
            }
            
            // For better visualization - slight level smoothing (to avoid sharp jumps)
            // In a real implementation this could be a more complex algorithm
            float current_level = mic_level_.load();
            float smoothed_level = current_level * 0.5f + peak * 0.5f; // More responsive smoothing
            mic_level_.store(smoothed_level);
            
            // Check if thread should exit before continuing
            if (!audio_thread_running_.load()) {
                Log(L"PollMicrophone: Thread signaled to exit, breaking");
                break;
            }
            
            hr = capture_client_->ReleaseBuffer(frames);
            if (FAILED(hr)) {
                Log(L"PollMicrophone: ReleaseBuffer failed, hr=0x" + std::to_wstring(hr));
                break;
            }
            
            // Check if thread should exit before getting next packet
            if (!audio_thread_running_.load()) {
                Log(L"PollMicrophone: Thread signaled to exit before next packet");
                break;
            }
            
            hr = capture_client_->GetNextPacketSize(&pkt_len);
            if (FAILED(hr)) {
                Log(L"PollMicrophone: GetNextPacketSize failed after processing, hr=0x" + std::to_wstring(hr));
                break;
            }
        }
    }
    catch (const std::exception& ex) {
        Log(L"PollMicrophone exception: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
    }
    catch (...) {
        Log(L"PollMicrophone: unknown exception");
    }
}

void ArgumentDebuggerWindow::Cleanup() {
    Log(L"Cleanup started");
    
    // Reset all ComPtr objects to automatically release resources.
    vertex_shader_.Reset();
    pixel_shader_.Reset();
    vertex_layout_.Reset();
    constant_buffer_.Reset();
    vertex_buffer_.Reset();
    index_buffer_.Reset();
    d3d_render_target_view_.Reset();
    swap_chain_.Reset();
    immediate_context_.Reset();
    d3d_device_.Reset();
    text_format_.Reset();
    small_text_format_.Reset(); // Release the small text format
    dwrite_factory_.Reset();
    d2d_render_target_.Reset();
    d2d_factory_.Reset();
    qr_bitmap_.Reset();
    
    // Audio thread and event are already closed in OnDestroy
    if (mix_format_) {
        CoTaskMemFree(mix_format_);
        mix_format_ = nullptr;
    }
    
    Log(L"Cleanup finished");
}

// Helper to load entire log file into loaded_data_ (showing only the last 100 lines)
void ArgumentDebuggerWindow::ShowLogs() {
    loaded_data_.clear();
    
    // File is already open for writing, so use _wfsopen with _SH_DENYNO flag
    // to explicitly allow both reading and writing from different handles
    FILE* log_file = _wfsopen(g_logPath.c_str(),
                              L"r, ccs=UTF-16LE",
                              _SH_DENYNO);
    if (!log_file) {
        command_status_ = L"Log file not found.";
        return;
    }
    
    // Ensure all pending writes are flushed to the file before reading
    fflush(g_log_file);
    
    // Skip BOM if present
    wint_t first_char = fgetwc(log_file);
    if (first_char != 0xFEFF) {
        // Not a BOM, rewind to start of file
        fseek(log_file, 0, SEEK_SET);
    }
    
    // Read all lines from the file first into a circular buffer of last 100 lines
    const size_t maxLines = 100;
    std::vector<std::wstring> lastLines;
    lastLines.reserve(maxLines);
    
    wchar_t buffer[1024];
    while (fgetws(buffer, 1024, log_file)) {
        if (lastLines.size() >= maxLines) {
            // Remove oldest line at the beginning
            lastLines.erase(lastLines.begin());
        }
        // Add new line at the end
        lastLines.push_back(buffer);
    }
    
    // Now build the output string with only the last 100 lines
    loaded_data_ = L"... (showing last " + std::to_wstring(lastLines.size()) + L" lines)\n\n";
    for (const auto& line : lastLines) {
        loaded_data_ += line;
    }
    
    fclose(log_file);
    command_status_ = L"Log file loaded (last 100 lines).";
}

// Method for saving data (timestamp and FPS) to %APPDATA%\ArgumentDebugger\saved_data.txt
void ArgumentDebuggerWindow::SaveData() {
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
    if (SUCCEEDED(hr)) {
        std::wstring folder_path = appdata_path;
        CoTaskMemFree(appdata_path);
        folder_path += L"\\ArgumentDebugger";
        // Create the directory if it does not exist yet.
        if (!CreateDirectoryW(folder_path.c_str(), nullptr)) {
            DWORD err = GetLastError();
            if (err != ERROR_ALREADY_EXISTS) {
                command_status_ = L"Error creating directory.";
                Log(L"SaveData: Error creating directory, code = " + std::to_wstring(err));
                return;
            }
        }
        std::wstring file_path = folder_path + L"\\saved_data.txt";
        FILE* file = nullptr;
        // Open the file for writing
        if (_wfopen_s(&file, file_path.c_str(), L"w, ccs=UTF-8") == 0 && file) {
            time_t timestamp = time(nullptr);
            // Write synced_fps_ so it matches the QR code value
            fwprintf(file, L"Timestamp: %lld\nFPS: %d\n",
                     static_cast<long long>(timestamp),
                     synced_fps_);
            fclose(file);
            command_status_ = L"Data saved successfully.";
            Log(L"SaveData: Data saved successfully to " + file_path);
        } else {
            command_status_ = L"Error opening file for writing.";
            Log(L"SaveData: Error opening file for writing: " + file_path);
        }
    }
    else {
        command_status_ = L"Error retrieving AppData path.";
        Log(L"SaveData: Error retrieving AppData path, hr = " + std::to_wstring(hr));
    }
}

// Method for loading data from %APPDATA%\ArgumentDebugger\saved_data.txt
void ArgumentDebuggerWindow::ReadData() {
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
    if (SUCCEEDED(hr)) {
        std::wstring folder_path = appdata_path;
        CoTaskMemFree(appdata_path);
        folder_path += L"\\ArgumentDebugger";
        std::wstring file_path = folder_path + L"\\saved_data.txt";
        FILE* file = nullptr;
        if (_wfopen_s(&file, file_path.c_str(), L"r, ccs=UTF-8") == 0 && file) {
            wchar_t buffer[256];
            loaded_data_.clear();
            // Read file contents line by line.
            while (fgetws(buffer, 256, file)) {
                loaded_data_ += buffer;
            }
            fclose(file);
            command_status_ = L"Data loaded successfully.";
            Log(L"ReadData: Data loaded successfully from " + file_path);
        }
        else {
            command_status_ = L"File not found.";
            loaded_data_.clear();
            Log(L"ReadData: File not found: " + file_path);
        }
    }
    else {
        command_status_ = L"Error retrieving AppData path.";
        Log(L"ReadData: Error retrieving AppData path, hr = " + std::to_wstring(hr));
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        Log(L"WM_CREATE");
        break;
    case WM_CHAR:
        Log(L"WM_CHAR: \"" + std::wstring(1, static_cast<wchar_t>(wparam)) + L"\"");
        if (g_app_instance && g_app_instance->is_running())
            g_app_instance->OnCharInput(static_cast<wchar_t>(wparam));
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && g_app_instance && g_app_instance->is_running())
            g_app_instance->OnCharInput(static_cast<wchar_t>(wparam));
        break;
    case WM_DESTROY:
        Log(L"WM_DESTROY");
        if (g_app_instance)
            g_app_instance->OnDestroy();
        return 0;
    default:
        return DefWindowProc(hwnd, message, wparam, lparam);
    }
    return 0;
}