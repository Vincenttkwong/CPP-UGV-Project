#pragma once

#include <ControllerInterface.h>
#include <UGVModule.h>
#include <SMObjects.h>

using namespace System;

ref class Controller : public UGVModule {
public:
    Controller(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl);
    void threadFunction() override;

    error_state processSharedMemory() override;
    
    void shutdownModules();
    bool getShutdownFlag() override;
    
    error_state processHeartbeats();

    ~Controller() {};

private:
    // Add any additional data members or helper functions here
    SM_ThreadManagement^ SM_TM_;
    SM_VehicleControl^ SM_VehicleControl_;

    ControllerInterface* ControllerInterface_;
    controllerState currentState;
    
    Stopwatch^ Watch;
};