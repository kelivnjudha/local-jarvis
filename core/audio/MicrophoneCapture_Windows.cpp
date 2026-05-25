#include "MicrophoneCapture_Windows.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <audioclient.h>
#include <combaseapi.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propkeydef.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <propidl.h>
#include <propsys.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace local_jarvis::audio {
namespace {

using Microsoft::WRL::ComPtr;

std::string wideToUtf8(const wchar_t *value)
{
    if (value == nullptr || value[0] == L'\0') {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring utf8ToWide(const std::string &value)
{
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 1) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

std::string hresultText(HRESULT hr, const char *context)
{
    std::ostringstream stream;
    stream << context << " failed with HRESULT 0x"
           << std::uppercase << std::hex << static_cast<unsigned long>(hr);
    return stream.str();
}

class ScopedCom {
public:
    ScopedCom()
        : m_hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
    }

    ~ScopedCom()
    {
        if (SUCCEEDED(m_hr)) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool ok() const
    {
        return SUCCEEDED(m_hr) || m_hr == RPC_E_CHANGED_MODE;
    }

    [[nodiscard]] HRESULT result() const
    {
        return m_hr;
    }

private:
    HRESULT m_hr;
};

std::optional<std::wstring> deviceIdFor(ComPtr<IMMDevice> device)
{
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || id == nullptr) {
        return std::nullopt;
    }

    std::wstring value(id);
    CoTaskMemFree(id);
    return value;
}

std::string friendlyNameFor(ComPtr<IMMDevice> device)
{
    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &properties))) {
        return "Microphone";
    }

    PROPVARIANT name;
    PropVariantInit(&name);
    std::string result = "Microphone";
    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR) {
        result = wideToUtf8(name.pwszVal);
    }
    PropVariantClear(&name);
    return result;
}

ComPtr<IMMDevice> selectedOrDefaultDevice(ComPtr<IMMDeviceEnumerator> enumerator, const std::string &selectedDeviceId)
{
    ComPtr<IMMDevice> device;
    if (!selectedDeviceId.empty()) {
        const std::wstring wideId = utf8ToWide(selectedDeviceId);
        if (!wideId.empty() && SUCCEEDED(enumerator->GetDevice(wideId.c_str(), &device)) && device) {
            return device;
        }
    }

    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device)) && device) {
        return device;
    }

    return {};
}

bool isFloatFormat(const WAVEFORMATEX *format)
{
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto *extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format);
        return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }
    return false;
}

bool isPcmFormat(const WAVEFORMATEX *format)
{
    if (format->wFormatTag == WAVE_FORMAT_PCM) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto *extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format);
        return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_PCM);
    }
    return false;
}

void appendPcmSamples(
    const BYTE *data,
    UINT32 frameCount,
    const WAVEFORMATEX *format,
    std::vector<float> &samples)
{
    const int channels = std::max<int>(1, format->nChannels);
    const int bitsPerSample = format->wBitsPerSample;
    const auto sampleCount = static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(channels);
    samples.clear();
    samples.reserve(sampleCount);

    if (isFloatFormat(format) && bitsPerSample == 32) {
        const auto *floatSamples = reinterpret_cast<const float *>(data);
        for (std::size_t index = 0; index < sampleCount; ++index) {
            samples.push_back(std::clamp(floatSamples[index], -1.0F, 1.0F));
        }
        return;
    }

    if (!isPcmFormat(format)) {
        samples.assign(sampleCount, 0.0F);
        return;
    }

    if (bitsPerSample == 16) {
        const auto *pcmSamples = reinterpret_cast<const std::int16_t *>(data);
        for (std::size_t index = 0; index < sampleCount; ++index) {
            samples.push_back(static_cast<float>(pcmSamples[index]) / 32768.0F);
        }
        return;
    }

    if (bitsPerSample == 24) {
        const int bytesPerSample = 3;
        for (std::size_t index = 0; index < sampleCount; ++index) {
            const BYTE *sample = data + (index * bytesPerSample);
            std::int32_t value = static_cast<std::int32_t>(sample[0])
                | (static_cast<std::int32_t>(sample[1]) << 8)
                | (static_cast<std::int32_t>(sample[2]) << 16);
            if ((value & 0x00800000) != 0) {
                value |= ~0x00FFFFFF;
            }
            samples.push_back(static_cast<float>(value) / 8388608.0F);
        }
        return;
    }

    if (bitsPerSample == 32) {
        const auto *pcmSamples = reinterpret_cast<const std::int32_t *>(data);
        for (std::size_t index = 0; index < sampleCount; ++index) {
            samples.push_back(static_cast<float>(static_cast<double>(pcmSamples[index]) / 2147483648.0));
        }
        return;
    }

    samples.assign(sampleCount, 0.0F);
}

} // namespace

WindowsMicrophoneCapture::WindowsMicrophoneCapture(const privacy::PrivacyManager &privacyManager)
    : m_privacyManager(privacyManager)
{
}

WindowsMicrophoneCapture::~WindowsMicrophoneCapture()
{
    stopMicrophoneCapture();
}

std::vector<AudioInputDevice> WindowsMicrophoneCapture::listInputDevices()
{
    ScopedCom com;
    if (!com.ok()) {
        setLastError(hresultText(com.result(), "COM initialization"));
        return {};
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        setLastError(hresultText(hr, "Creating audio device enumerator"));
        return {};
    }

    std::wstring defaultId;
    ComPtr<IMMDevice> defaultDevice;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &defaultDevice)) && defaultDevice) {
        if (auto id = deviceIdFor(defaultDevice)) {
            defaultId = *id;
        }
    }

    ComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) {
        setLastError(hresultText(hr, "Enumerating microphone devices"));
        return {};
    }

    UINT count = 0;
    collection->GetCount(&count);

    std::vector<AudioInputDevice> devices;
    devices.reserve(count);
    for (UINT index = 0; index < count; ++index) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(index, &device)) || !device) {
            continue;
        }

        const auto id = deviceIdFor(device);
        if (!id.has_value()) {
            continue;
        }

        devices.push_back(AudioInputDevice {
            .id = wideToUtf8(id->c_str()),
            .displayName = friendlyNameFor(device),
            .isDefault = *id == defaultId,
            .isAvailable = true
        });
    }

    if (devices.empty()) {
        setLastError("No active Windows microphone input devices were found.");
    } else {
        setLastError("");
    }
    return devices;
}

bool WindowsMicrophoneCapture::selectInputDevice(const std::string &deviceId)
{
    std::lock_guard lock(m_mutex);
    m_selectedDeviceId = deviceId;
    return true;
}

std::string WindowsMicrophoneCapture::selectedInputDeviceId() const
{
    std::lock_guard lock(m_mutex);
    return m_selectedDeviceId;
}

bool WindowsMicrophoneCapture::startMicrophoneCapture()
{
    if (!m_privacyManager.captureStatus().microphoneEnabled) {
        setLastError("");
        return false;
    }

    {
        std::lock_guard lock(m_mutex);
        if (m_microphoneActive) {
            return true;
        }
    }

    stopMicrophoneCapture();

    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = false;
        m_startCompleted = false;
        m_lastError.clear();
        m_levelMeter.reset();
    }

    m_worker = std::thread([this]() {
        captureLoop();
    });

    std::unique_lock lock(m_mutex);
    m_startCondition.wait(lock, [this]() {
        return m_startCompleted;
    });
    const bool started = m_microphoneActive;
    lock.unlock();

    if (!started && m_worker.joinable()) {
        m_worker.join();
    }

    return started;
}

void WindowsMicrophoneCapture::stopMicrophoneCapture()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = true;
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }

    std::lock_guard lock(m_mutex);
    m_microphoneActive = false;
    m_startCompleted = false;
    m_stopRequested = false;
    m_levelMeter.reset();
}

bool WindowsMicrophoneCapture::startSystemAudioCapture()
{
    if (m_privacyManager.captureStatus().systemAudioEnabled) {
        setLastError("System audio capture is outside Phase 3A scope.");
    }
    return false;
}

void WindowsMicrophoneCapture::stopSystemAudioCapture()
{
    std::lock_guard lock(m_mutex);
    m_systemAudioActive = false;
}

bool WindowsMicrophoneCapture::isMicrophoneActive() const
{
    std::lock_guard lock(m_mutex);
    return m_microphoneActive;
}

bool WindowsMicrophoneCapture::isSystemAudioActive() const
{
    std::lock_guard lock(m_mutex);
    return m_systemAudioActive;
}

void WindowsMicrophoneCapture::setTranscriptCallback(TranscriptCallback callback)
{
    std::lock_guard lock(m_mutex);
    m_transcriptCallback = std::move(callback);
}

double WindowsMicrophoneCapture::currentInputLevel() const
{
    std::lock_guard lock(m_mutex);
    return m_levelMeter.level();
}

std::string WindowsMicrophoneCapture::lastError() const
{
    std::lock_guard lock(m_mutex);
    return m_lastError;
}

void WindowsMicrophoneCapture::captureLoop()
{
    ScopedCom com;
    if (!com.ok()) {
        completeStart(false, hresultText(com.result(), "COM initialization"));
        return;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        completeStart(false, hresultText(hr, "Creating audio device enumerator"));
        return;
    }

    std::string selectedId;
    {
        std::lock_guard lock(m_mutex);
        selectedId = m_selectedDeviceId;
    }

    ComPtr<IMMDevice> device = selectedOrDefaultDevice(enumerator, selectedId);
    if (!device) {
        completeStart(false, "No Windows microphone input device is available.");
        return;
    }

    ComPtr<IAudioClient> audioClient;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient);
    if (FAILED(hr)) {
        completeStart(false, hresultText(hr, "Activating microphone audio client"));
        return;
    }

    WAVEFORMATEX *mixFormat = nullptr;
    hr = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr) || mixFormat == nullptr) {
        completeStart(false, hresultText(hr, "Reading microphone mix format"));
        return;
    }

    const REFERENCE_TIME bufferDuration = 1'000'000; // 100 ms.
    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        mixFormat,
        nullptr);
    if (FAILED(hr)) {
        CoTaskMemFree(mixFormat);
        completeStart(false, hresultText(hr, "Initializing microphone audio client"));
        return;
    }

    HANDLE captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (captureEvent == nullptr) {
        CoTaskMemFree(mixFormat);
        completeStart(false, "Failed to create microphone capture event.");
        return;
    }

    hr = audioClient->SetEventHandle(captureEvent);
    if (FAILED(hr)) {
        CloseHandle(captureEvent);
        CoTaskMemFree(mixFormat);
        completeStart(false, hresultText(hr, "Setting microphone event handle"));
        return;
    }

    ComPtr<IAudioCaptureClient> captureClient;
    hr = audioClient->GetService(IID_PPV_ARGS(&captureClient));
    if (FAILED(hr)) {
        CloseHandle(captureEvent);
        CoTaskMemFree(mixFormat);
        completeStart(false, hresultText(hr, "Opening microphone capture client"));
        return;
    }

    hr = audioClient->Start();
    if (FAILED(hr)) {
        CloseHandle(captureEvent);
        CoTaskMemFree(mixFormat);
        completeStart(false, hresultText(hr, "Starting microphone capture"));
        return;
    }

    completeStart(true, "");

    std::vector<float> samples;
    bool captureError = false;
    while (!stopRequested() && !captureError) {
        const DWORD waitResult = WaitForSingleObject(captureEvent, 100);
        if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_TIMEOUT) {
            setLastError("Microphone capture wait failed.");
            break;
        }

        UINT32 packetFrames = 0;
        hr = captureClient->GetNextPacketSize(&packetFrames);
        if (FAILED(hr)) {
            setLastError(hresultText(hr, "Reading microphone packet size"));
            break;
        }

        while (packetFrames > 0) {
            BYTE *data = nullptr;
            UINT32 frameCount = 0;
            DWORD flags = 0;
            hr = captureClient->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                setLastError(hresultText(hr, "Reading microphone packet"));
                captureError = true;
                break;
            }

            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
                samples.assign(static_cast<std::size_t>(frameCount) * std::max<int>(1, mixFormat->nChannels), 0.0F);
            } else {
                appendPcmSamples(data, frameCount, mixFormat, samples);
            }

            {
                std::lock_guard lock(m_mutex);
                m_levelMeter.processSamples(samples);
            }

            captureClient->ReleaseBuffer(frameCount);
            hr = captureClient->GetNextPacketSize(&packetFrames);
            if (FAILED(hr)) {
                setLastError(hresultText(hr, "Reading microphone packet size"));
                captureError = true;
                packetFrames = 0;
                break;
            }
        }
    }

    audioClient->Stop();
    CloseHandle(captureEvent);
    CoTaskMemFree(mixFormat);

    std::lock_guard lock(m_mutex);
    m_microphoneActive = false;
    m_levelMeter.reset();
}

void WindowsMicrophoneCapture::completeStart(bool ok, const std::string &error)
{
    {
        std::lock_guard lock(m_mutex);
        m_microphoneActive = ok;
        m_startCompleted = true;
        m_lastError = error;
    }
    m_startCondition.notify_all();
}

bool WindowsMicrophoneCapture::stopRequested() const
{
    std::lock_guard lock(m_mutex);
    return m_stopRequested;
}

void WindowsMicrophoneCapture::setLastError(const std::string &error)
{
    std::lock_guard lock(m_mutex);
    m_lastError = error;
}

} // namespace local_jarvis::audio
