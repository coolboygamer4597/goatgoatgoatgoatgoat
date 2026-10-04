#pragma once
#include <windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

class memory_t final {
public:
    memory_t() = default;
    ~memory_t() = default;

    memory_t(const memory_t&) = delete;
    memory_t& operator=(const memory_t&) = delete;

    std::uint32_t find_process_id(const std::string& process_name);
    std::uint64_t find_module_address(const std::string& module_name);
    bool attach_to_process(const std::string& process_name);
    bool attach_to_process_read_only(const std::string& process_name);

    std::string read_string(std::uint64_t address);
    bool IsConnected() const;

    template <typename T>
    T read(std::uint64_t address) const {
        T buffer{};
        if (process_handle && process_handle != INVALID_HANDLE_VALUE)
            ReadProcessMemory(process_handle, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), nullptr);
        return buffer;
    }

    template <typename T>
    void write(std::uint64_t address, const T& value) const {
        if (process_handle && process_handle != INVALID_HANDLE_VALUE)
            WriteProcessMemory(process_handle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), nullptr);
    }

    bool read_raw(std::uint64_t address, void* buffer, std::size_t size) const;
    bool write_raw(std::uint64_t address, const void* buffer, std::size_t size) const;

    template <typename T>
    T Read(std::uint64_t address) const { return read<T>(address); }

    template <typename T>
    bool Write(std::uint64_t address, const T& value) const {
        SIZE_T done = 0;
        return IsConnected() && WriteProcessMemory(process_handle,
            reinterpret_cast<LPVOID>(address), &value, sizeof(T), &done) &&
            done == sizeof(T);
    }

    SIZE_T ReadRaw(std::uint64_t address, void* buffer, SIZE_T size) const;
    SIZE_T WriteRaw(std::uint64_t address, const void* buffer, SIZE_T size) const;
    std::uint64_t GetModuleBase(const wchar_t* module_name = nullptr) const;
    HANDLE GetHandle() const { return process_handle; }
    bool IsValid(std::uint64_t address) const;
    std::uint64_t Alloc(SIZE_T size, DWORD protection = PAGE_EXECUTE_READWRITE) const;
    void Free(std::uint64_t address) const;
    bool Protect(std::uint64_t address, SIZE_T size, DWORD protection,
                 DWORD* old_protection = nullptr) const;

    std::uint32_t get_process_id() const { return process_id; }
    std::uint64_t get_module_address() const { return base_address; }
    HANDLE get_process_handle() const { return process_handle; }

private:
    std::uint32_t process_id = 0;
    std::uint64_t base_address = 0;
    HANDLE process_handle = nullptr;
};

extern std::unique_ptr<memory_t> memory;
