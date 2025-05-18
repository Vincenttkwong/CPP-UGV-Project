#pragma once

#include <NetworkedModule.h>
#include <SMObjects.h>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class Laser : public NetworkedModule
{
public:
	Laser(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser);
    void threadFunction() override;
    
    error_state connect(String^ hostName, int portNumber) override;
    error_state communicate() override;
    error_state processSharedMemory() override;

    void shutdownModules();
    bool getShutdownFlag() override;

    error_state processHeartbeats();

    ~Laser() {};

private:
    SM_ThreadManagement^ SM_TM_;
    SM_Laser^ SM_Laser_;
    array<unsigned char>^ SendData;
    Stopwatch^ Watch;
};

