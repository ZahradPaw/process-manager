#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <conio.h>

#ifdef UNICODE
#define tcout std::wcout
#else 
#define tcout std::cout
#endif // UNICODE

// Функция вывода информации о процессе 
void PrintProcessInfo(PROCESSENTRY32& processEntry) {
    // Открытие процесса для получения его дескриптора
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processEntry.th32ProcessID);

	// Если есть доступ к процессу, данные о нем выведутся в консоль
    if (hProcess) {

        // ID и имя процесса
        tcout << std::left << std::setw(10) << processEntry.th32ProcessID
                           << std::setw(50) << processEntry.szExeFile;

        // Объём памяти в KB
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) 
            tcout << std::left << std::setw(20) << pmc.WorkingSetSize / 1024;

        // Число открытых ресурсов
        DWORD handleCount;
        if (GetProcessHandleCount(hProcess, &handleCount)) 
            tcout << std::left << std::setw(20) << handleCount;

        // Число чтений, записей, кол-во прочитанных байт и записанных байт
        IO_COUNTERS ioCounters;
        if (GetProcessIoCounters(hProcess, &ioCounters)) {
            tcout << std::left << std::setw(20) << ioCounters.ReadOperationCount
                << std::setw(20) << ioCounters.WriteOperationCount
                << std::setw(20) << ioCounters.ReadTransferCount
                << std::setw(20) << ioCounters.WriteTransferCount << std::endl;
        }
		
		CloseHandle(hProcess);
    }
}

// Функция уничтожения выбранного процесса и всех его дочерних процессов
bool KillProcessAndChildren(DWORD processID) {

    // Создание нового снимка процессов
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (!hSnapshot) 
        return false;

    PROCESSENTRY32 processEntry = { 0 };
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    // Рекурсивное уничтожение дочерних процессов
    if (Process32First(hSnapshot, &processEntry)) {

        do {
            if (processEntry.th32ParentProcessID == processID) 
                KillProcessAndChildren(processEntry.th32ProcessID);
        } while (Process32Next(hSnapshot, &processEntry));

    }

    CloseHandle(hSnapshot);

    // Уничтожение самого процесса
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);

    if (hProcess) {
        if (!TerminateProcess(hProcess, 0)) 
            return false;
        CloseHandle(hProcess);
        return true;
    }
    return false;
}

// Функция проверки введеного ID процесса на корректность
bool isValidProcessID(const std::string& inputID) {

    // Проверка, что введеная строка не пустая, состоит из цифр и длиной меньше 10
    if (inputID.empty() or inputID.size() > 10) 
        return false;
    
    for (char ch : inputID) {
        if (!std::isdigit(ch)) {
            return false;
        }
    }

    DWORD pid = std::stoul(inputID);

    return (pid <= MAXDWORD) and (pid > 4);
}

int main() {
    setlocale(LC_ALL, "RU");

    // Установка размера окна консоли на полный экран
    HWND hWnd = GetForegroundWindow();
    ShowWindow(hWnd, SW_MAXIMIZE);
    CloseHandle(hWnd);

    // Установка имени окна
    SetConsoleTitle(L"Просмотр активных процессов в памяти");

    bool running = true;

    // Основной цикл программы
    do {
        system("cls");

        std::cout << "Программа для просмотра активных процессов в памяти и их уничтожения\n\n";

        // Создание снимка процессов
        HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        // Проверка, удалось ли получить снимок процессов
        if (!processSnapshot) {
            std::cerr << "Ошибка: не удалось получить снимок процессов.\n";
            system("pause");
            return 1;
        }

        PROCESSENTRY32 processEntry = { 0 };
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        // Попытка получить информацию о первом процессе из снимка
        if (!Process32First(processSnapshot, &processEntry)) {
            std::cerr << "Ошибка: не удалось получить информацию о процессах.\n";
            system("pause");
            return 2;
        }

        // Вывод начала таблицы с информацией об процессах
        std::cout << std::left
            << std::setw(10) << "PID"
            << std::setw(50) << "Имя процесса"
            << std::setw(20) << "Объем памяти (KB)"
            << std::setw(20) << "Открытых ресурсов"
            << std::setw(20) << "Чтений"
            << std::setw(20) << "Записей"
            << std::setw(20) << "Прочитано байт"
            << std::setw(20) << "Записано байт" << std::endl;

        std::cout << std::string(180, '-') << std::endl;

        // Вывод через цикл информации о всех процессах из снимка
        do {
            PrintProcessInfo(processEntry);
        } while (Process32Next(processSnapshot, &processEntry));

        CloseHandle(processSnapshot);

        std::cout << std::string(180, '-') << std::endl << std::endl;

        std::cout << "Выберите действие (нажмите клавишу): q - выход из программы, r - обновить список, t - уничтожить процесс.\n";

        // Ожидание действий пользователя 
        char ch;
        while (_kbhit()) _getch();
        while (true) {
            if (_kbhit()) {
                ch = _getch();
                if ((ch == 'q') or (ch == 'r') or (ch == 't'))
                    break;
            }
        }

        // Завершение программы
        if (ch == 'q') 
            running = false;

        // Уничтожение процесса по его ID
        else if (ch == 't') {
            std::string input;
            std::cout << "\nВведите ID процесса из списка для его уничтожения: ";
            std::cin >> input;

            // Проверка корректности ввода и попытка уничтожения выбранного процесса
            if (isValidProcessID(input)) {

                DWORD processID = std::stoul(input);

                if (KillProcessAndChildren(processID))
                    std::cout << "\nПроцесс с ID " << processID << " успешно заврешён.\n";
                else
                    std::cerr << "\nОшибка: процесс с ID " << processID << " не удалось завершить.\n";
            }
            else 
                std::cout << "\nОшибка: неверный формат введеных данных. Вводите корректный ID процесса.\n";

            std::cout << std::endl;
            system("pause");
        }

    } while (running);

    return 0;
}