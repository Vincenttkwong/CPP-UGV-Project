#include "Controller.h"

#using <System.dll>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

Controller::Controller(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl)
{
	SM_TM_ = SM_TM;
	SM_VehicleControl_ = SM_VehicleControl;
	ControllerInterface_ = new ControllerInterface(1,0); // Change to 0 on real machine
	Watch = gcnew Stopwatch;
}

void Controller::threadFunction()
{
	//Console::WriteLine("Controller thread is starting");
	// setup the stopwatch
	Watch = gcnew Stopwatch;

	// wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();

	// Start the stopwatch
	Watch->Start();

	// Start thread loop
	while (!getShutdownFlag())
	{
		//Console::WriteLine("Controller thread is running.");
		processHeartbeats();

		int maxAttempts = 5;
		while (maxAttempts > 0)
		{
			if (processSharedMemory() == SUCCESS)
			{
				//Console::WriteLine("Processed Controller shared memory!");
				break;
			}
			else
			{
				Console::WriteLine("Process Controller shared memory failed. Retrying...");
				maxAttempts--;
			}
		}

		if (maxAttempts == 0)
		{
			Console::WriteLine("Max attempts at processing Controller shared memory. Shutting down all modules...");
			shutdownModules();
		}

		Thread::Sleep(20);
	}
	// If xbox controller disconnects, set Speed and Steering to 0
	Monitor::Enter(SM_VehicleControl_->lockObject);
	SM_VehicleControl_->Speed = 0;
	SM_VehicleControl_->Steering = 0;
	Monitor::Exit(SM_VehicleControl_->lockObject);
	Console::WriteLine("Controller thread is terminating");
}

error_state Controller::processSharedMemory()
{
	if (ControllerInterface_->IsConnected())
	{
		currentState = ControllerInterface_->GetState();
		//ControllerInterface_->printControllerState(currentState);

		//Console::WriteLine("Speed: {0}", SM_VehicleControl_->Speed);
		//Console::WriteLine("Steering: {0}", SM_VehicleControl_->Steering);

		// Control Speed to Shared Memory
		SM_VehicleControl_->Speed = (currentState.rightTrigger != 0) ? currentState.rightTrigger : -currentState.leftTrigger;

		// Control Steering to Shared Memory
		SM_VehicleControl_->Steering = -40 * currentState.rightThumbX;
		return SUCCESS;
	}
	else
	{
		return ERR_CONNECTION;
	}
}

void Controller::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool Controller::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_CONTROLLER;
}

error_state Controller::processHeartbeats()
{
	if ((SM_TM_->heartbeat & bit_CONTROLLER) == 0)
	{
		SM_TM_->heartbeat |= bit_CONTROLLER;
		Watch->Restart();
	}
	else
	{
		if (Watch->ElapsedMilliseconds > CRASH_LIMIT)
		{
			shutdownModules();
			return ERR_TMM_FAILURE;
		}
	}
	return SUCCESS;
}