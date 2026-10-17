#include <windows.h>
#include <iostream>
#include <vector>

struct nvmlProcessInfo_v1 {
    unsigned int pid;
    unsigned long long usedGpuMemory;
};

struct nvmlProcessInfo_v2 {
    unsigned int pid;
    unsigned long long usedGpuMemory;
    unsigned int gpuInstanceId;
    unsigned int computeInstanceId;
};

int main() {
    HMODULE hNvml = LoadLibraryA("nvml.dll");
    if (!hNvml) {
        std::cout << "Failed to load nvml.dll\n";
        return 1;
    }

    auto nvmlInit = (int(*)())GetProcAddress(hNvml, "nvmlInit_v2");
    auto nvmlDeviceGetHandleByIndex = (int(*)(unsigned int, void**))GetProcAddress(hNvml, "nvmlDeviceGetHandleByIndex_v2");
    
    FARPROC pGfx_v3 = GetProcAddress(hNvml, "nvmlDeviceGetGraphicsRunningProcesses_v3");
    FARPROC pGfx_v2 = GetProcAddress(hNvml, "nvmlDeviceGetGraphicsRunningProcesses_v2");
    FARPROC pGfx_v1 = GetProcAddress(hNvml, "nvmlDeviceGetGraphicsRunningProcesses");

    FARPROC pComp_v3 = GetProcAddress(hNvml, "nvmlDeviceGetComputeRunningProcesses_v3");
    FARPROC pComp_v2 = GetProcAddress(hNvml, "nvmlDeviceGetComputeRunningProcesses_v2");
    FARPROC pComp_v1 = GetProcAddress(hNvml, "nvmlDeviceGetComputeRunningProcesses");

    std::cout << "Gfx v3: " << (void*)pGfx_v3 << " v2: " << (void*)pGfx_v2 << " v1: " << (void*)pGfx_v1 << "\n";
    std::cout << "Comp v3: " << (void*)pComp_v3 << " v2: " << (void*)pComp_v2 << " v1: " << (void*)pComp_v1 << "\n";

    if (nvmlInit() != 0) {
        std::cout << "nvmlInit failed\n";
        return 1;
    }

    void* dev = nullptr;
    if (nvmlDeviceGetHandleByIndex(0, &dev) != 0) {
        std::cout << "nvmlDeviceGetHandleByIndex failed\n";
        return 1;
    }

    // Test v3
    if (pGfx_v3) {
        unsigned int count = 0;
        int res = ((int(*)(void*, unsigned int*, void*))pGfx_v3)(dev, &count, nullptr);
        std::cout << "pGfx_v3 probe count: res=" << res << ", count=" << count << "\n";
        if (count > 0) {
            std::vector<nvmlProcessInfo_v2> procs(count);
            res = ((int(*)(void*, unsigned int*, void*))pGfx_v3)(dev, &count, procs.data());
            std::cout << "pGfx_v3 with v2 struct: res=" << res << "\n";
            for (unsigned int i = 0; i < count; i++) {
                std::cout << "  pid=" << procs[i].pid << " mem=" << procs[i].usedGpuMemory << "\n";
            }
        }
    }

    // Test v2
    if (pGfx_v2) {
        unsigned int count = 0;
        int res = ((int(*)(void*, unsigned int*, void*))pGfx_v2)(dev, &count, nullptr);
        std::cout << "pGfx_v2 probe count: res=" << res << ", count=" << count << "\n";
        if (count > 0) {
            std::vector<nvmlProcessInfo_v1> procs(count);
            res = ((int(*)(void*, unsigned int*, void*))pGfx_v2)(dev, &count, procs.data());
            std::cout << "pGfx_v2 with v1 struct: res=" << res << "\n";
            for (unsigned int i = 0; i < count; i++) {
                std::cout << "  pid=" << procs[i].pid << " mem=" << procs[i].usedGpuMemory << "\n";
            }
        }
    }

    // Test v1
    if (pGfx_v1) {
        unsigned int count = 0;
        int res = ((int(*)(void*, unsigned int*, void*))pGfx_v1)(dev, &count, nullptr);
        std::cout << "pGfx_v1 probe count: res=" << res << ", count=" << count << "\n";
        if (count > 0) {
            std::vector<nvmlProcessInfo_v1> procs(count);
            res = ((int(*)(void*, unsigned int*, void*))pGfx_v1)(dev, &count, procs.data());
            std::cout << "pGfx_v1 with v1 struct: res=" << res << "\n";
            for (unsigned int i = 0; i < count; i++) {
                std::cout << "  pid=" << procs[i].pid << " mem=" << procs[i].usedGpuMemory << "\n";
            }
        }
    }

    return 0;
}
