#include <iostream>
#include <Windows.h> // ��� ������ � �������� � ����������
#include <TlHelp32.h> // ��� ��������� �������� � ��������� ���������� � �� ��������
#include <Psapi.h> // ��� ��������� ���������� � ��������� (������������ GetProcessMemoryInfo)
#include <vector>

// ���� ��������� ���������� ������� Unicode, �� ����� ����� ����� wcout, ��� �������� �� ����� Unicode
// ����� ����� ����� ����� ������ cout ��� ascii 
#ifdef UNICODE
#define tcout std::wcout
#else 
#define tcout std::cout
#endif // UNICODE


// ����� ������ �����������, ��� ��� ������� ���� ������ ��� �����������
class SmartHandle {
public:

    SmartHandle(HANDLE _handle) : _handle(_handle) {}

    ~SmartHandle() {
        // ��� ������ ������ ���������� ����� ��������� ��� ������������ ��������
        if (_handle) {
            CloseHandle(_handle);
        }
    }

    // ���������� � bool
    operator bool() {
        return _handle != NULL;
    }

    // ���������� � ������ HANDLE
    operator HANDLE() {
        return _handle;
    }

    // ������ 
    HANDLE get_handle() {
        return _handle;
    }

private:

    HANDLE _handle = NULL;

};


// ���������, ������� ����� ������� ���������� � ��������� 
struct ProcessInfo {
    PROCESSENTRY32 proccesEntry; // ��������� ������ ������ ��� �������� ��������
    // �� ������� ���������� ��������� �������, ����� ������� ���������� � ��� � ����������
    std::vector<THREADENTRY32> threads; // ������ ����� ������� ���������, �������� ���� ��� ������
};

// ������� ����������� �������� � ��������� id � ���� ��� �������� ���������
bool KillProcessAndChildren_help(DWORD processID) {
    // ������� �������� ������ ��������� � ��������� �������� ������ � ������� 
    SmartHandle hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    // ������� ����� ���������� ��� �������� ��������, ������������ �� ���� ������� ���������
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (pe.th32ParentProcessID == processID) {
                // ���� th32ParentProcessID ������ id ������������� ��������
                KillProcessAndChildren_help(pe.th32ProcessID); 
                // ���������� �������� ��� �� ������� ��� ����������� ��������� �������� � ��� �������� ���������
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    // ����� ����� �������� ���������� ��������, ������� ����� ����������, � ������� �������� ����� ������� PROCESS_TERMINATE
    SmartHandle hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);
    if (hProcess) {
        // ���� ������� ������� � ����������, �� ����������
        TerminateProcess(hProcess, 0); // ������� ���������� ������� �� ���������� �����������, 0 - ��� ��������� ����������
        // ������� ���� true, ���� ������� ��� ������� ���������
    }
}

int main_func() {
    // � ������� ������� ������ ����������, ���������� ������� ���� ������� ��������� (��������� ������ ������)
    SmartHandle processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    // 0 - ��������� ������� (� ������)
    // ����� �������� ������� ���� ������� (������ ������ ����)
    SmartHandle threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    // ���������, ���������� �� �������� ������� ��������� � ������ (��� ���, ��� ������ NULL)
    if (!processSnapshot or !threadSnapshot) {
        return 1;
    }

    // ������ ����� ������� ��������� � ������ ����� � ���� ��������� 
    std::vector<ProcessInfo> processesInfo;

    // ��������� ���������� � ��������� 

    std::vector<PROCESSENTRY32> processes; // ������ ����� ������� ��������� � ����� ��� ������� �������� 
    PROCESSENTRY32 processEntry; // ��������� ����� ������� ���������� � �������� 
    processEntry.dwSize = sizeof(PROCESSENTRY32); // ��� ��������� �������� ����� ������� ���� ������� ��������� � ������
    // ����� ��� ���������������� ��� �������� ���������: PROCESSENTRY32 proccesEntry = { sizeof(PROCESSENTRY32) };

    // ���������� �������� ������ ������� �������� � ��������, ������� �� �� ��������
    if (!Process32First(processSnapshot, &processEntry)) {
        // ������� �������� ������� ���������, � ����� ������ �� ���������, ���� ������� ������ ������� ��������
        // ������� ����� true, ���� ������� ������� �������, ����� false
        return 2;
    }

    // ����� �������� �� ���� ���������
    do {
        // ������ �������� ��������� � ����� � �������� ��������� � ������ � ����
        processes.push_back(processEntry);
    } while (Process32Next(processSnapshot, &processEntry));
    // ������� ��������� � ����. ��������, ��������� � ������ � ��������� ��-������ � ����� false, ���� �� ������ ���

    // ����� ������� ������� ����� �������� ���������� � �������

    std::vector<THREADENTRY32> threads; // ������ ����� ������� ��������� � ����������� � �������
    THREADENTRY32 threadEntry = { sizeof(THREADENTRY32) }; // ��������� ������ ���� ��� �����

    // ���������� ��� � ����������, ������� ����� �� ��, �� ��� �������, � ��������� ��������� ��� ��� 
    if (!Thread32First(threadSnapshot, &threadEntry)) {
        return 3;
    }

    // ������ �� ���� ������� � ������ �� ������ � ������ 
    do {
        threads.push_back(threadEntry);
    } while (Thread32Next(threadSnapshot, &threadEntry));

    // ����� ��� �������� ����� ���������� ��� ������, �������� ����� ����
    for (const auto& process : processes) {
        std::vector<THREADENTRY32> subThreads; // ����� ����� ���������� ������ ������� ��� �������� ��������
        for (const auto& thread : threads) {
            if (process.th32ProcessID == thread.th32OwnerProcessID) {
                // ���� id �������� �������� � id �������� �������� ������ ���������, �� ������� ����� � ������
                subThreads.push_back(thread);
            }
        }
        // ������� ������ � �������� � ��� ������� � ������ � ������ ����������� � ������ ����� � ���������
        processesInfo.push_back(ProcessInfo{ process, subThreads });
        // �������������� ���� � ������� �������� ������� � ���� ��������� ProcessInfo
    }
    // ! ��� ������, ������� ������� ������������ ������ �������, ����� ���������, ��� ���������� id ������� �������� � ����

    // ����� ������� � ������� ����� ��������� ����� ���� szExeFile �������� proccesEntry
    for (const auto& processInfo : processesInfo) {
        tcout << processInfo.proccesEntry.szExeFile << std::endl;
        // tcout - �������� ����� ������������� � ������ �������� ��. 

        // ��� ������� �������� ������� ������ ��� �������
        for (const auto& thread : processInfo.threads) {
            tcout << '\t' << thread.th32ThreadID << std::endl;
        }
    }

    // ����� ����� �������� ���� � ������ ��������
    for (const auto& process : processes) {

        // ������� �������� ����� ������� (���������, ��� ����� �������� ���������� � �������� � �������) � ����� id ��������
        SmartHandle hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process.th32ProcessID);
        // ���������� ���������� ������ ��������
        tcout << process.szExeFile << process.th32ProcessID << '\t';

        // ����������� ������ ������������ ������ ���������

        PROCESS_MEMORY_COUNTERS pmc; // ��������� ����� ������� ���� � ������������� ������ ���������
        // ����� ������� ���������� ������ � ������ ��������, �������� ��� ����������, ������ �� ��������� ���� � ������ � � ������
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            // ���� WorkingSetSize ������ ������������ ��������� ����������� ������ � ������ 
            tcout << (pmc.WorkingSetSize / 1024) << L" KB" << '\t';
        }

        // ����� ����������� ���-�� ������, �������, ���������� � ����������� ������

        IO_COUNTERS ioCounters; // ��������� ����� ������� ������ � ����������� ������� ���������
        // ������� ���������� ������ � ������� ��������, �������� ��� ���������� � ������ �� ��������� ���� � �������/�������
        if (GetProcessIoCounters(hProcess, &ioCounters)) {
            // ���� ����� true, �� ������� ��� ������ � �������/������� ��������
            tcout << ioCounters.ReadOperationCount << ' '; // ���-�� ������ �������� 
            tcout << ioCounters.WriteOperationCount << ' '; // ���-�� �������
            tcout << ioCounters.ReadTransferCount << ' '; // ���-�� ����������� ���
            tcout << ioCounters.WriteTransferCount << ' '; // ���-�� ���������� ���
        }

        // ����� ������� ���-�� �������� �������� ���������
        DWORD handleCount; // ���-�� �������� �������� (������������)
        if (GetProcessHandleCount(hProcess, &handleCount)) {
            // ������� �������� ���������� ��������, � ����� ������ �� ��, ���� ����� �������� ���-�� �������� ��������
            tcout << handleCount << std::endl;
        }
    }
}