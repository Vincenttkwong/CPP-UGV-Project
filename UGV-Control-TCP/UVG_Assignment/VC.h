#pragma once

#include <NetworkedModule.h>
#include <SMObjects.h>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class VehicleControl : public NetworkedModule
{
public:
    VehicleControl(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl);\
    void threadFunction() override;

    error_state connect(String^ hostName, int portNumber) override;
    error_state communicate() override;
    error_state processSharedMemory() override;

    void shutdownModules();
    bool getShutdownFlag() override;

    error_state processHeartbeat();

    ~VehicleControl() {};

private:
    SM_ThreadManagement^ SM_TM_;
    SM_VehicleControl^ SM_VehicleControl_;
    array<unsigned char>^ SendData;
    int flag;
    Stopwatch^ Watch;
};
