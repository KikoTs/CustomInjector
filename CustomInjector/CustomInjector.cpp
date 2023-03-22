#include <Windows.h>
#include <iostream>
#include <string>
#include <fstream>

BOOL FileExists(const std::wstring& filePath)
{
    std::ifstream file(filePath.c_str());
    return file.good();
}

BOOL InjectDLL(DWORD processID, const std::wstring& dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, processID);
    if (hProcess == NULL)
    {
        std::cerr << "Error: Could not open process " << processID << std::endl;
        return FALSE;
    }

    LPVOID lpLoadLibraryW = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    if (lpLoadLibraryW == NULL)
    {
        std::cerr << "Error: Could not find LoadLibraryW function" << std::endl;
        CloseHandle(hProcess);
        return FALSE;
    }

    LPVOID lpRemoteDllPath = VirtualAllocEx(hProcess, NULL, (dllPath.length() + 1) * sizeof(wchar_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (lpRemoteDllPath == NULL)
    {
        std::cerr << "Error: Could not allocate memory in target process" << std::endl;
        CloseHandle(hProcess);
        return FALSE;
    }

    if (!WriteProcessMemory(hProcess, lpRemoteDllPath, dllPath.c_str(), (dllPath.length() + 1) * sizeof(wchar_t), NULL))
    {
        std::cerr << "Error: Could not write DLL path to target process memory" << std::endl;
        VirtualFreeEx(hProcess, lpRemoteDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)lpLoadLibraryW, lpRemoteDllPath, 0, NULL);
    if (hRemoteThread == NULL)
    {
        std::cerr << "Error: Could not create remote thread" << std::endl;
        VirtualFreeEx(hProcess, lpRemoteDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    WaitForSingleObject(hRemoteThread, INFINITE);
    CloseHandle(hRemoteThread);
    VirtualFreeEx(hProcess, lpRemoteDllPath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return TRUE;
}


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: CustomInjector.exe <program.exe> <inject.dll>" << std::endl;
        return 1;
    }

    std::wstring exePath = std::wstring(argv[1], argv[1] + strlen(argv[1]));
    std::wstring dllPath = std::wstring(argv[2], argv[2] + strlen(argv[2]));

    if (!FileExists(exePath))
    {
        std::cerr << "Error: Exe file not found" << std::endl;
        return 1;
    }
    if (!FileExists(dllPath))
    {
        std::cerr << "Error: DLL file not found" << std::endl;
        return 1;
    }
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;

    // Create the target application in a suspended state
    if (!CreateProcess(
        exePath.c_str(),
        nullptr, nullptr, nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr, nullptr,
        &si, &pi
    ))
    {
        std::cerr << "Error: Could not start target application" << std::endl;
        return 1;
    }

    if (InjectDLL(pi.dwProcessId, dllPath))
    {
        std::cout << "DLL injected successfully" << std::endl;
        // Resume the main thread of the target application
        ResumeThread(pi.hThread);
    }
    else
    {
        std::cerr << "DLL injection failed" << std::endl;
        TerminateProcess(pi.hProcess, 1); // Terminate the target application
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}
