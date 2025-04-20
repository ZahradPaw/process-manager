#include <iostream>
#include <Windows.h> // для работы с потоками и процессами
#include <TlHelp32.h> // для получения снапшота с текущимим процессами и их потоками
#include <Psapi.h> // для получения информации о процессах (используется GetProcessMemoryInfo)
#include <vector>

// если программа использует символы Unicode, то вывод будет через wcout, что отвечает за вывод Unicode
// иначе будет вывод через просто cout для ascii 
#ifdef UNICODE
#define tcout std::wcout
#else 
#define tcout std::cout
#endif // UNICODE


// класс умного дескриптора, что сам очищает свою память при уничтожении
class SmartHandle {
public:

    SmartHandle(HANDLE _handle) : _handle(_handle) {}

    ~SmartHandle() {
        // при выходе всегда дескриптор нужно закрывать для освобождения ресурсов
        if (_handle) {
            CloseHandle(_handle);
        }
    }

    // приведение к bool
    operator bool() {
        return _handle != NULL;
    }

    // приведение к самому HANDLE
    operator HANDLE() {
        return _handle;
    }

    // геттер 
    HANDLE get_handle() {
        return _handle;
    }

private:

    HANDLE _handle = NULL;

};


// структура, которая будет хранить информацию о процессах 
struct ProcessInfo {
    PROCESSENTRY32 proccesEntry; // структура хранит данные для описания процесса
    // на процесс приходится несколько потоков, нужно хранить информацию о них в контейнере
    std::vector<THREADENTRY32> threads; // вектор будет хранить структуры, хранящие инфу про потоки
};

// функция уничтожения процесса с указанным id и всех его дочерних процессов
bool KillProcessAndChildren_help(DWORD processID) {
    // сначала создадим снимок процессов и структуру хранения данных о поцессе 
    SmartHandle hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    // сначала нужно уничтожить все дочерние процессы, пробежавшись по всем текущим процессам
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (pe.th32ParentProcessID == processID) {
                // поле th32ParentProcessID хранит id родительского процесса
                KillProcessAndChildren_help(pe.th32ProcessID); 
                // рекурсивно вызываем эту же функцию для уничтожения дочернего процесса и его дочерних процессов
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    // далее нужно получить дескриптор процесса, который нужно уничтожить, в функции доступом нужно указать PROCESS_TERMINATE
    SmartHandle hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);
    if (hProcess) {
        // если процесс получен в дескриптор, то уничтожаем
        TerminateProcess(hProcess, 0); // функция уничтожает процесс по указанному дескриптору, 0 - код успешного заврешения
        // функция верёт true, если процесс был успещно уничтожен
    }
}

int main_func() {
    // с помощью функции создаём дескриптор, содержащий снапшот всех текущих процессов (установка нужных флагов)
    SmartHandle processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    // 0 - начальный процесс (с начала)
    // также получаем снапшот всех потоков (указав другой флаг)
    SmartHandle threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    // проверяем, получилось ли получить снапшот процессов и потока (есл нет, они вернут NULL)
    if (!processSnapshot or !threadSnapshot) {
        return 1;
    }

    // вектор будет хранить структуры с полной инфой о всех процессах 
    std::vector<ProcessInfo> processesInfo;

    // получение информации о процессах 

    std::vector<PROCESSENTRY32> processes; // вектор будет хранить структуры с инфой для каждого процесса 
    PROCESSENTRY32 processEntry; // структура будет хранить информацию о процессе 
    processEntry.dwSize = sizeof(PROCESSENTRY32); // для поддержки платформ нужно указать полю размера струкутры её размер
    // можно ещё инициализировать при создании структуры: PROCESSENTRY32 proccesEntry = { sizeof(PROCESSENTRY32) };

    // попытаемся получить данные первого процесса и проверим, удалось ли их получить
    if (!Process32First(processSnapshot, &processEntry)) {
        // функция получает снапшот процессов, а также ссылку на структуру, куда запишет данные первого процесса
        // функция вернёт true, если успешно запишет процесс, иначе false
        return 2;
    }

    // далее пройдёмся по всем процессам
    do {
        // каждую итерацию структуру с инфой о процессе сохраняем в вектор с ними
        processes.push_back(processEntry);
    } while (Process32Next(processSnapshot, &processEntry));
    // функция переходит к след. процессу, возвращая её данные в структуру по-ссылке и вернёт false, если их больше нет

    // далее похожим образом будем получать информацию о потоках

    std::vector<THREADENTRY32> threads; // вектор будет хранить структруы с информацией о потоках
    THREADENTRY32 threadEntry = { sizeof(THREADENTRY32) }; // структура хрнаит инфу про поток

    // аналагично как с процессами, функции почти те же, но для потоков, и используя структуры для них 
    if (!Thread32First(threadSnapshot, &threadEntry)) {
        return 3;
    }

    // проход по всем потокам и запись их данных в вектор 
    do {
        threads.push_back(threadEntry);
    } while (Thread32Next(threadSnapshot, &threadEntry));

    // далее для процесса нужно определить его потоки, перебрав через цикл
    for (const auto& process : processes) {
        std::vector<THREADENTRY32> subThreads; // здесь будут сохранятся данные потоков для текущего процесса
        for (const auto& thread : threads) {
            if (process.th32ProcessID == thread.th32OwnerProcessID) {
                // если id текущего процесса и id процесса текущего потока совпадают, то заносим поток в вектор
                subThreads.push_back(thread);
            }
        }
        // передаём данные о процессе и его потоках в вектор с нашими структурами с полной инфой о процессах
        processesInfo.push_back(ProcessInfo{ process, subThreads });
        // инициализируем поля с данными текущими данными в нашу структуру ProcessInfo
    }
    // ! это пример, быстрее сначала сформировать список потоков, потом процессов, где сравниваем id потоков процесса с ними

    // далее выводим в консоль имена процессов через поля szExeFile структур proccesEntry
    for (const auto& processInfo : processesInfo) {
        tcout << processInfo.proccesEntry.szExeFile << std::endl;
        // tcout - определён через предпроцессор в начале програмы см. 

        // для каждого процесса выведим номера его потоков
        for (const auto& thread : processInfo.threads) {
            tcout << '\t' << thread.th32ThreadID << std::endl;
        }
    }

    // далее можно получить инфу о каждом процессе
    for (const auto& process : processes) {

        // функция получает флаги доступа (указываем, что нужно получить информацию о процессе и записях) а также id процесса
        SmartHandle hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process.th32ProcessID);
        // вовзарщает дескриптор самого процесса
        tcout << process.szExeFile << process.th32ProcessID << '\t';

        // определение объёма используемой памяти процессом

        PROCESS_MEMORY_COUNTERS pmc; // структура будет хранить инфу о использовании памяти процессом
        // далее функция записывает данные о памяти процесса, принимая его дескриптор, ссылку на структуру инфы о памяти и её размер
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            // поле WorkingSetSize хранит используемую процессом оперативную память в байтах 
            tcout << (pmc.WorkingSetSize / 1024) << L" KB" << '\t';
        }

        // далее определение кол-ва чтений, записей, записанных и прочитанных байтов

        IO_COUNTERS ioCounters; // структура будет хранить данные о количествах записей процессом
        // функция записывает данные о записях процесса, принимая его дескриптор и ссылку на структуру инфы о чтениях/записях
        if (GetProcessIoCounters(hProcess, &ioCounters)) {
            // если вернёт true, то выведем все данные о записях/чтениях процесса
            tcout << ioCounters.ReadOperationCount << ' '; // кол-во чтений процесса 
            tcout << ioCounters.WriteOperationCount << ' '; // кол-во записей
            tcout << ioCounters.ReadTransferCount << ' '; // кол-во прочитанных бит
            tcout << ioCounters.WriteTransferCount << ' '; // кол-во записанных бит
        }

        // после выведим кол-во открытых ресурсов процессом
        DWORD handleCount; // кол-во открытых ресурсов (дескрипторов)
        if (GetProcessHandleCount(hProcess, &handleCount)) {
            // функция получает дескриптор процесса, а также ссылку на то, куда будет записано кол-во открытых ресурсов
            tcout << handleCount << std::endl;
        }
    }
}