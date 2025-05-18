#pragma once

#include <UGVModule.h>
#include <SMObjects.h>

ref struct ThreadProperties
{
    ThreadStart^ ThreadStart_;
    bool Critical;
    String^ ThreadName;
    uint8_t BitID;

    ThreadProperties(ThreadStart^ start, bool crit, uint8_t bit_id, String^ threadName)
    {
        ThreadStart_ = start;
        Critical = crit;
        ThreadName = threadName;
        BitID = bit_id;
    }
};

ref class ThreadManagement : public UGVModule
{
public:
    // Create shared memory objects
    error_state setupSharedMemory();

    // Thread function for PMM
    void threadFunction() override;

    // Send/Recieve data from shared memory structures
    error_state processSharedMemory() override;

    void shutdownModules();

    // Get Shutdown signal for module, from Thread Management SM
    bool getShutdownFlag() override;

    // Additional Functions
    error_state processHeartbeats();

    ~ThreadManagement() {};

private:
    // Add any additional data members or helper functions here
    SM_Laser^ SM_Laser_;
    SM_GPS^ SM_GPS_;
    SM_VehicleControl^ SM_VehicleControl_;
    array<Thread^>^ ThreadList;
    array<ThreadProperties^>^ ThreadPropertiesList;
    
};
