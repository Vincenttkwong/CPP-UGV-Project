#pragma once

#include <NetworkedModule.h>
#include <SMObjects.h>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

#define CRC32_POLYNOMIAL 0xEDB88320L

ref class GPS : public NetworkedModule
{
public:
    GPS(SM_ThreadManagement^ SM_TM, SM_GPS^ SM_GPS);
    void threadFunction() override;

    error_state connect(String^ hostName, int portNumber) override;
    error_state communicate() override;
    error_state processSharedMemory() override;
    
    void shutdownModules();
    bool getShutdownFlag() override;
    
    error_state processHeartbeat();

    unsigned long CRC32Value(int i);
    unsigned long CalculateBlockCRC32(unsigned long ulCount, unsigned char* ucBuffer);

    ~GPS() {};

private:
    SM_ThreadManagement^ SM_TM_;
    SM_GPS^ SM_GPS_;
    array<unsigned char>^ SendData;
    Stopwatch^ Watch;
};
