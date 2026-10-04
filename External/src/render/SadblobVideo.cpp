#include "SadblobVideo.h"
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>
#include <cstring>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;
namespace SadblobVideo {
namespace {
constexpr const wchar_t* kFiles[StyleCount] = {
    L"mitsuha-watching-shooting-stars-your-name-moewalls-com.mp4",
    L"hoshino-heterochromatic-gaze-blue-archive-moewalls-com.mp4",
    L"ryo-yamada-shooting-star-bocchi-the-rock-moewalls-com.mp4",
    L"takanashi-hoshino-in-class-looking-at-the-sky-moewalls-com.mp4",
    L"kessoku-band-show-bocchi-the-rock-moewalls-com.mp4",
    L"nijika-ijichi-fireworks-bocchi-the-rock-moewalls-com.mp4",
    L"night-of-floating-lanterns-and-fireworks-moewalls-com.mp4"
};
constexpr const char* kNames[StyleCount] = {
    "Mitsuha - Shooting Stars", "Hoshino - Heterochromatic Gaze",
    "Ryo - Shooting Star", "Hoshino - Classroom Sky",
    "Kessoku Band", "Nijika - Fireworks", "Floating Lanterns"
};

std::atomic<bool> running{false};
std::atomic<bool> requested{false};
std::atomic<int> requestedStyle{0};
std::atomic<unsigned> requestedGeneration{0};
std::atomic<bool> missing{false};
std::thread worker;
std::mutex frameMutex;
std::vector<unsigned char> frame;
unsigned frameWidth = 0, frameHeight = 0;
unsigned long long frameSerial = 0;
int frameStyle = -1;
unsigned frameGeneration = 0;
unsigned long long uploadedSerial = 0;
int uploadedStyle = -1;
unsigned uploadedGeneration = 0;
std::mutex pathMutex;
std::wstring customPath;
ComPtr<ID3D11Texture2D> texture;
ComPtr<ID3D11ShaderResourceView> srv;
unsigned textureWidth = 0, textureHeight = 0;

const std::filesystem::path& BundleDirectory() {
    static const std::filesystem::path directory = [] {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return std::filesystem::path(exe).parent_path() / L"Sadblob";
    }();
    return directory;
}

std::filesystem::path VideoPath(int style) {
    if (style == StyleCount) {
        std::lock_guard<std::mutex> lock(pathMutex);
        return customPath;
    }
    return BundleDirectory() / kFiles[style];
}

void DecodeLoop() {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninit = SUCCEEDED(co);
    if (FAILED(co) || FAILED(MFStartup(MF_VERSION))) {
        missing.store(true);
        if (uninit) CoUninitialize();
        return;
    }
    while (running.load()) {
        if (!requested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        const int style = requestedStyle.load();
        const unsigned generation = requestedGeneration.load();
        const auto path = VideoPath(style);
        if (path.empty() || !std::filesystem::is_regular_file(path)) {
            missing.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        ComPtr<IMFAttributes> attrs;
        ComPtr<IMFSourceReader> reader;
        ComPtr<IMFMediaType> output;
        HRESULT hr = MFCreateAttributes(&attrs, 1);
        if (SUCCEEDED(hr)) hr = attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromURL(path.c_str(), attrs.Get(), &reader);
        if (SUCCEEDED(hr)) hr = MFCreateMediaType(&output);
        if (SUCCEEDED(hr)) hr = output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(hr)) hr = output->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (SUCCEEDED(hr)) hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output.Get());
        ComPtr<IMFMediaType> actual;
        if (SUCCEEDED(hr)) hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actual);
        UINT32 width = 0, height = 0;
        if (SUCCEEDED(hr)) hr = MFGetAttributeSize(actual.Get(), MF_MT_FRAME_SIZE, &width, &height);
        if (FAILED(hr) || width == 0 || height == 0 || width > 8192 || height > 8192) {
            missing.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        missing.store(false);
        const unsigned outW = (std::min)(width, 960u);
        const unsigned outH = (std::min)(height, (std::max)(1u, height * outW / width));
        const auto start = std::chrono::steady_clock::now();
        LONGLONG firstTime = -1;
        LONGLONG lastPublished = -1;
        while (running.load() && requested.load() && requestedStyle.load() == style &&
               requestedGeneration.load() == generation) {
            DWORD stream = 0, flags = 0;
            LONGLONG sampleTime = 0;
            ComPtr<IMFSample> sample;
            hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &stream, &flags, &sampleTime, &sample);
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
            if (!sample) continue;
            if (firstTime < 0) firstTime = sampleTime;
            const auto due = start + std::chrono::nanoseconds((sampleTime - firstTime) * 100);
            while (running.load() && requested.load() && requestedStyle.load() == style &&
                   requestedGeneration.load() == generation &&
                   std::chrono::steady_clock::now() < due)
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            if (!running.load() || !requested.load() || requestedStyle.load() != style ||
                requestedGeneration.load() != generation) break;
            if (lastPublished >= 0 && sampleTime - lastPublished < 333333) continue;

            ComPtr<IMFMediaBuffer> buffer;
            if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;
            BYTE* pixels = nullptr;
            DWORD maxLength = 0, length = 0;
            if (FAILED(buffer->Lock(&pixels, &maxLength, &length))) continue;
            const size_t stride = size_t(width) * 4;
            if (length >= stride * height) {
                std::vector<unsigned char> scaled(size_t(outW) * outH * 4);
                for (unsigned y = 0; y < outH; ++y) {
                    const unsigned srcY = y * height / outH;
                    const BYTE* src = pixels + size_t(srcY) * stride;
                    BYTE* dst = scaled.data() + size_t(y) * outW * 4;
                    for (unsigned x = 0; x < outW; ++x) {
                        const BYTE* p = src + size_t(x * width / outW) * 4;
                        BYTE* q = dst + size_t(x) * 4;
                        q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; q[3] = 255;
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(frameMutex);
                    frame.swap(scaled);
                    frameWidth = outW; frameHeight = outH;
                    frameStyle = style;
                    frameGeneration = generation;
                    ++frameSerial;
                }
                lastPublished = sampleTime;
            }
            buffer->Unlock();
        }

        if (requestedStyle.load() == style && requested.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    MFShutdown();
    CoUninitialize();
}
}

const char* StyleName(int index) { return kNames[(std::max)(0, (std::min)(StyleCount - 1, index))]; }
bool HasBundledClips() {
    static const bool available = [] {
        for (const auto* name : kFiles)
            if (!std::filesystem::is_regular_file(BundleDirectory() / name)) return false;
        return true;
    }();
    return available;
}
void SetCustomPath(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(pathMutex);
    if (customPath == path) return;
    customPath = path;
    requestedGeneration.fetch_add(1);
}
bool MissingFile() { return missing.load(); }
ID3D11ShaderResourceView* Texture() {
    return uploadedStyle == requestedStyle.load() && uploadedGeneration == requestedGeneration.load()
        ? srv.Get() : nullptr;
}
float AspectRatio() { return textureHeight ? float(textureWidth) / float(textureHeight) : 1.0f; }

void Update(ID3D11Device* device, ID3D11DeviceContext* context, bool active, int style) {
    style = (std::max)(0, (std::min)(StyleCount, style));
    requestedStyle.store(style);
    requested.store(active);
    if (active && !running.exchange(true)) worker = std::thread(DecodeLoop);
    if (!active || !device || !context) return;
    std::lock_guard<std::mutex> lock(frameMutex);
    if (frameStyle != style || frameGeneration != requestedGeneration.load() ||
        (uploadedSerial == frameSerial && uploadedStyle == style && uploadedGeneration == frameGeneration) ||
        frame.empty()) return;
    if (textureWidth != frameWidth || textureHeight != frameHeight || !texture) {
        srv.Reset(); texture.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = frameWidth; desc.Height = frameHeight; desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture))) return;
        if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &srv))) return;
        textureWidth = frameWidth; textureHeight = frameHeight;
    }
    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(context->Map(texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
    for (unsigned y = 0; y < frameHeight; ++y)
        std::memcpy(static_cast<BYTE*>(map.pData) + size_t(y) * map.RowPitch,
                    frame.data() + size_t(y) * frameWidth * 4, size_t(frameWidth) * 4);
    context->Unmap(texture.Get(), 0);
    uploadedSerial = frameSerial;
    uploadedStyle = style;
    uploadedGeneration = frameGeneration;
}

void Shutdown() {
    requested.store(false);
    if (running.exchange(false) && worker.joinable()) worker.join();
    srv.Reset(); texture.Reset();
    textureWidth = textureHeight = 0;
}
}
