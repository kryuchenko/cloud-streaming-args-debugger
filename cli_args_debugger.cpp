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

// Link with required libraries
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// Include QrCodeGen (ensure that qrcodegen.hpp and qrcodegen.cpp are in your project)
#include "qrcodegen.hpp"
using qrcodegen::QrCode;

// Use Microsoft::WRL::ComPtr for COM object management
using Microsoft::WRL::ComPtr;

// Helper macro that calls a DirectX function and throws an exception on failure.
#define DX_CALL(expr, msg) do { auto hr = (expr); if (FAILED(hr)) throw std::runtime_error(msg); } while(0)

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

constexpr const wchar_t* kWindowClassName = L"ArgumentDebuggerClass";
constexpr const wchar_t* kWindowCaption   = L"Argument Debugger";

// Description text for the window
const std::vector<std::wstring> kDescriptionLines = {
    L"Argument Debugger",
    L"This utility displays all command-line arguments in a full-screen window.",
    L"Type 'exit' and press Enter to quit."
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

private:
    void InitializeWindow(HINSTANCE h_instance, int cmd_show);
    void InitializeDevice();
    void CreateDeviceAndSwapChain(UINT width, UINT height);
    void CreateRenderTargetView();
    void CreateD2DResources();
    void CreateShadersAndGeometry();
    void RenderFrame();
    void Cleanup();
    void UpdateRotation(float delta_time);
    void UpdateQrCode(ULONGLONG current_time);

    HWND window_handle_ = nullptr;
    bool is_running_ = true;
    ULONGLONG last_time_ = 0;
    float rotation_angle_ = 0.0f;
    std::wstring user_input_;
    std::vector<std::wstring> args_;

    // Variables for the QR code and FPS
    ComPtr<ID2D1Bitmap> qr_bitmap_;
    ULONGLONG last_qr_update_time_ = 0;
    float current_fps_ = 0.0f;

    // Direct2D and DirectWrite objects
    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<ID2D1RenderTarget> d2d_render_target_;
    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<IDWriteTextFormat> text_format_;

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
};

int WINAPI wWinMain(HINSTANCE h_instance, HINSTANCE, PWSTR, int cmd_show) {
    try {
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
        return exit_code;
    }
    catch (const std::exception &ex) {
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
    MSG msg = {};
    last_time_ = GetTickCount64();

    while (is_running_) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                is_running_ = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        RenderFrame();
    }
    return static_cast<int>(msg.wParam);
}

void ArgumentDebuggerWindow::OnCharInput(wchar_t ch) {
    if (ch == VK_RETURN) {
        if (_wcsicmp(user_input_.c_str(), L"exit") == 0) {
            PostQuitMessage(0);
            is_running_ = false;
        }
        user_input_.clear();
    }
    else if (ch == VK_BACK) {
        if (!user_input_.empty())
            user_input_.pop_back();
    }
    else if (ch == VK_ESCAPE) {
        PostQuitMessage(0);
        is_running_ = false;
    }
    else {
        user_input_ += ch;
    }
}

void ArgumentDebuggerWindow::OnDestroy() {
    PostQuitMessage(0);
    is_running_ = false;
}

void ArgumentDebuggerWindow::InitializeWindow(HINSTANCE h_instance, int /*cmd_show*/) {
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

    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
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
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    }};
    DX_CALL(
        d3d_device_->CreateInputLayout(layout_desc.data(), static_cast<UINT>(layout_desc.size()),
                                         vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                         vertex_layout_.GetAddressOf()),
        "Failed to create input layout."
    );
    immediate_context_->IASetInputLayout(vertex_layout_.Get());

    // Define vertices.
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
    if (current_time - last_qr_update_time_ < 5000)
        return; // Throttle updates to once every 5 seconds.
    last_qr_update_time_ = current_time;

    time_t unix_time = time(nullptr);
    std::ostringstream oss;
    oss << "t=" << unix_time << ";f=" << static_cast<int>(current_fps_);
    if (!args_.empty()) {
        oss << ";args=";
        for (const auto& arg : args_) {
            // Convert wide string to narrow string before streaming.
            oss << wstring_to_string(arg) << " ";
        }
    }
    std::string qr_data = oss.str();

    // Use QrCodeGen to create a standard QR code with error correction level MEDIUM.
    QrCode qr = QrCode::encodeText(qr_data.c_str(), QrCode::Ecc::MEDIUM);
    int qr_modules = qr.getSize();
    constexpr int pixel_size = 375;  // QR code size: 375x375 pixels
    float scale = static_cast<float>(pixel_size) / qr_modules;
    std::vector<uint32_t> pixels(pixel_size * pixel_size, 0xffffffff); // White background

    for (int y = 0; y < pixel_size; y++) {
        for (int x = 0; x < pixel_size; x++) {
            int module_x = static_cast<int>(x / scale);
            int module_y = static_cast<int>(y / scale);
            if (module_x < qr_modules && module_y < qr_modules) {
                if (qr.getModule(module_x, module_y))
                    pixels[y * pixel_size + x] = 0xff000000; // Black
            }
        }
    }

    D2D1_BITMAP_PROPERTIES bitmapProperties = {};
    bitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    bitmapProperties.dpiX = 96.0f;
    bitmapProperties.dpiY = 96.0f;

    // Reset previous bitmap.
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

    // Update FPS (instantaneous value)
    current_fps_ = (delta_time > 0.0f) ? (1.0f / delta_time) : 0.0f;

    UpdateRotation(delta_time);

    // Clear the Direct3D render target.
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
        d2d_render_target_->DrawTextW(line.c_str(), static_cast<UINT32>(line.size()),
                                       text_format_.Get(), rect, white_brush.Get());
        y_pos += kLineHeight;
    }
    y_pos += kLineHeight;

    // Draw command-line argument header.
    std::wstring cli_text = args_.empty() 
        ? L"No arguments were received." 
        : L"Received the following arguments:";
    {
        D2D1_RECT_F rect = D2D1::RectF(kMargin, y_pos, size.width - kMargin, y_pos + kLineHeight);
        d2d_render_target_->DrawTextW(cli_text.c_str(), static_cast<UINT32>(cli_text.size()),
                                       text_format_.Get(), rect, green_brush.Get());
    }
    y_pos += kLineHeight;

    // Draw the actual command-line arguments (if any).
    if (!args_.empty()) {
        std::wstring joined_args;
        for (const auto& arg : args_) {
            joined_args += arg + L" ";
        }
        D2D1_RECT_F args_rect = D2D1::RectF(kMargin, y_pos, size.width - kMargin, size.height - 200.0f);
        d2d_render_target_->DrawTextW(joined_args.c_str(), static_cast<UINT32>(joined_args.size()),
                                       text_format_.Get(), args_rect, green_brush.Get());
        y_pos += kLineHeight;
    }

    y_pos += 10.0f;
    // Move the "Character count:" text 50 pixels lower to avoid overlap.
    y_pos += 50.0f;
    size_t char_count = 0;
    for (const auto& arg : args_) {
        char_count += arg.size() + 1;
    }
    if (char_count > 0)
        --char_count;
    std::wstring count_str = L"Character count: " + std::to_wstring(char_count);
    {
        D2D1_RECT_F rect = D2D1::RectF(kMargin, y_pos, size.width - kMargin, y_pos + kLineHeight);
        d2d_render_target_->DrawTextW(count_str.c_str(), static_cast<UINT32>(count_str.size()),
                                       text_format_.Get(), rect, green_brush.Get());
    }

    std::wstring exit_prompt = L"Type 'exit' and press Enter to quit:";
    D2D1_RECT_F exit_prompt_rect = D2D1::RectF(kMargin, size.height - 100.0f, size.width - kMargin, size.height - 70.0f);
    d2d_render_target_->DrawTextW(exit_prompt.c_str(), static_cast<UINT32>(exit_prompt.size()),
                                   text_format_.Get(), exit_prompt_rect, yellow_brush.Get());

    D2D1_RECT_F user_input_rect = D2D1::RectF(kMargin, size.height - 60.0f, size.width - kMargin, size.height - 30.0f);
    d2d_render_target_->DrawTextW(user_input_.c_str(), static_cast<UINT32>(user_input_.size()),
                                   text_format_.Get(), user_input_rect, green_brush.Get());

    // Draw the QR code in the lower-left corner.
    // The left and bottom margins remain fixed at 60 pixels,
    // the QR code size is 375×375 pixels, and it is raised by 100 pixels.
    if (qr_bitmap_) {
        constexpr int qr_size = 375;
        constexpr float qr_margin = 60.0f; // fixed margin
        float qr_x = qr_margin;
        float qr_y = size.height - qr_size - qr_margin - 100.0f;
        d2d_render_target_->DrawBitmap(qr_bitmap_.Get(), D2D1::RectF(qr_x, qr_y, qr_x + qr_size, qr_y + qr_size));
    }

    // End Direct2D drawing
    DX_CALL(d2d_render_target_->EndDraw(), "Failed to end Direct2D draw.");

    // Present the frame.
    swap_chain_->Present(1, 0);
}

void ArgumentDebuggerWindow::UpdateRotation(float delta_time) {
    rotation_angle_ += delta_time * DirectX::XM_PIDIV4 / 2.0f;
}

void ArgumentDebuggerWindow::Cleanup() {
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
    dwrite_factory_.Reset();
    d2d_render_target_.Reset();
    d2d_factory_.Reset();
    qr_bitmap_.Reset();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CHAR:
        if (g_app_instance && g_app_instance->is_running())
            g_app_instance->OnCharInput(static_cast<wchar_t>(wparam));
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && g_app_instance && g_app_instance->is_running())
            g_app_instance->OnCharInput(static_cast<wchar_t>(wparam));
        break;
    case WM_DESTROY:
        if (g_app_instance)
            g_app_instance->OnDestroy();
        return 0;
    default:
        return DefWindowProc(hwnd, message, wparam, lparam);
    }
    return 0;
}
