#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include "../include/hardware/GpuController.hpp"

int main() {
    auto snap = GpuController::Query();
    std::wcout << L"Model: " << snap.model << L"\n";
    std::wcout << L"Power: " << snap.powerWatts << L" W, Temp: " << snap.temperatureC << L" C\n";
    std::wcout << L"IsAwake: " << (snap.isAwake ? L"true" : L"false") << L"\n";
    std::wcout << L"Processes (" << snap.processes.size() << L"):\n";
    for (const auto& p : snap.processes) {
        std::wcout << L"  - " << p.name << L" (PID " << p.pid << L"), Mem: " << p.memoryMB << L" MB\n";
    }
    return 0;
}
