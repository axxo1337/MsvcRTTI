#include <cerrno>

#include <iostream>
#include <iomanip>

#include <Windows.h>
#include <TlHelp32.h>

#include "MsvcRTTI.h"

DWORD getProcessID(const char *process_name);
uintptr_t getProcessModuleAddress(const char *module_name, DWORD process_id);

int main(int argc, char **argv)
{
    std::cout << "\n============MsvcRTTI external POC============\n";

    if (argc < 3)
    {
        std::cerr << "[-] Missing arguments, format: <PocMsvcRTTI> <process_name> <module_name | object_address> <object_address (if module_name specified)>" << std::endl;
        return 1;
    }

    DWORD process_id{getProcessID(argv[1])};

    if (process_id == 0)
    {
        std::cerr << "[-] Failed to fetch process ID" << std::endl;
        return 1;
    }

    HANDLE h_process{OpenProcess(PROCESS_ALL_ACCESS, false, process_id)};

    if (h_process == INVALID_HANDLE_VALUE)
    {
        std::cerr << "[-] Failed to open handle to process" << std::endl;
        return 1;
    }

    uintptr_t image_base{};
    uintptr_t p_object{};

    if (argc == 3)
    {
        p_object = std::strtoull(argv[2], nullptr, 16);

        if (!p_object)
        {
            std::cerr << "[-] Failed to parse object address in fourth argument" << std::endl;
            return 1;
        }

        image_base = getProcessModuleAddress(argv[1], process_id);
    }
    else
    {
        p_object = std::strtoull(argv[3], nullptr, 16);

        if (!p_object)
        {
            std::cerr << "[-] Failed to parse object address in fifth argument" << std::endl;
            return 1;
        }

        image_base = getProcessModuleAddress(argv[2], process_id);
    }

    auto base_class_names{MsvcRTTI::extractAllBaseClassNamesExternal(h_process, image_base, (void *)p_object)};

    for (int i{}; i < base_class_names.size(); i++)
    {
        auto &base_class_name{base_class_names[i]};
        std::cout << "[+] " << std::setw(2) << i << " - " << base_class_name << "\n";
    }

    // End formating to be pretty
    std::cout << "=============================================\n";

    return 0;
}

DWORD getProcessID(const char *process_name)
{
    std::wstring w_process_name(process_name, process_name + strlen(process_name));
    HANDLE h_snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};

    if (h_snapshot == INVALID_HANDLE_VALUE)
    {
        std::cerr << "[-] Failed to create snaphost\n";
        return 1;
    }

    PROCESSENTRY32 process_entry;
    process_entry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(h_snapshot, &process_entry))
    {
        do
        {
            if (w_process_name.compare(process_entry.szExeFile) == 0)
            {
                CloseHandle(h_snapshot);
                return process_entry.th32ProcessID;
            }
        } while (Process32Next(h_snapshot, &process_entry));
    }

    CloseHandle(h_snapshot);

    return 0;
}

uintptr_t getProcessModuleAddress(const char *module_name, DWORD process_id)
{
    std::wstring w_module_name(module_name, module_name + strlen(module_name));
    HANDLE h_snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_id)};

    if (h_snapshot == INVALID_HANDLE_VALUE)
    {
        std::cerr << "[-] Failed to create snaphost\n";
        return 1;
    }

    MODULEENTRY32 module_entry;
    module_entry.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(h_snapshot, &module_entry))
    {
        do
        {
            if (w_module_name.compare(module_entry.szModule) == 0)
            {
                CloseHandle(h_snapshot);
                return (uintptr_t)module_entry.modBaseAddr;
            }
        } while (Module32Next(h_snapshot, &module_entry));
    }

    CloseHandle(h_snapshot);

    return 0;
}