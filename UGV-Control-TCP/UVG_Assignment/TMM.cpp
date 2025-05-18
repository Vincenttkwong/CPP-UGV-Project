#include <conio.h>

#include "Controller.h"
#include "TMM.h"
#include "Display.h"
#include "GNSS.h"
#include "Laser.h"
#include "VC.h"

#using <System.dll>

// Create shared memory objects
error_state ThreadManagement::setupSharedMemory()
{
	SM_TM_ = gcnew SM_ThreadManagement;
	SM_Laser_ = gcnew SM_Laser;
	SM_GPS_ = gcnew SM_GPS;
	SM_VehicleControl_ = gcnew SM_VehicleControl;
	return SUCCESS;
}

// Thread function for TMM
void ThreadManagement::threadFunction()
{
	//Console::WriteLine("TMT Thread is starting. ");
	// make a list of thread properties
	ThreadPropertiesList = gcnew array<ThreadProperties^>{
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Laser(SM_TM_, SM_Laser_), &Laser::threadFunction), true, bit_LASER, "Laser thread"),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew GPS(SM_TM_, SM_GPS_), &GPS::threadFunction), false, bit_GPS, "GNSS thread"),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Controller(SM_TM_, SM_VehicleControl_), &Controller::threadFunction), true, bit_CONTROLLER, "Controller thread"),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew VehicleControl(SM_TM_, SM_VehicleControl_), &VehicleControl::threadFunction), true, bit_VC, "VehicleControl thread"),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Display(SM_TM_, SM_Laser_), &Display::threadFunction), true, bit_DISPLAY, "Display thread")
	};

	// make a list of threads
	ThreadList = gcnew array<Thread^>(ThreadPropertiesList->Length);
	// make the stopwatch list
	SM_TM_->WatchList = gcnew array<Stopwatch^>(ThreadPropertiesList->Length);
	// make ThreadBarrier;
	SM_TM_->ThreadBarrier = gcnew Barrier(ThreadPropertiesList->Length + 1);

	// start all the threads
	for (int i = 0; i < ThreadPropertiesList->Length; i++)
	{
		SM_TM_->WatchList[i] = gcnew Stopwatch;
		ThreadList[i] = gcnew Thread(ThreadPropertiesList[i]->ThreadStart_);
		ThreadList[i]->Start();
	}

	// wait at the TMM thread barrier
	SM_TM_->ThreadBarrier->SignalAndWait();

	// start all the stop watches
	for (int i = 0; i < ThreadList->Length; i++)
	{
		SM_TM_->WatchList[i]->Start();
	}

	while (!getShutdownFlag())
	{
		//Console::WriteLine("TMT Thread is running.");
		processHeartbeats();

		// Check if a key is pressed
		if (_kbhit())
		{
			int key = _getch();
			if (key == 'q' || key == 'Q')
			{
				// 'q' key pressed, initiate shutdown
				shutdownModules();
				//Console::WriteLine("Shutting down threads...");
			}
		}

		// Keep checking the heartbeats
		Thread::Sleep(50);
	}

	// join all threads 
	for (int i = 0; i < ThreadPropertiesList->Length; i++)
	{
		ThreadList[i]->Join();
	}
	//Console::WriteLine("TMT Thread terminating ...");
}

// Send/Recieve data from memory structures
error_state ThreadManagement::processSharedMemory()
{
	return SUCCESS;
}

void ThreadManagement::shutdownModules()
{
	SM_TM_->shutdown = 0xFF; //0b11111111
}

// Get Shutdown signal for module, from Thread Management SM
bool ThreadManagement::getShutdownFlag()
{
	return (SM_TM_->shutdown & bit_PM);
}



error_state ThreadManagement::processHeartbeats()
{
	for (int i = 0; i < ThreadList->Length; i++)
	{
		// check the heartbeat flag of the ith thread (is it high (happy))
		if (SM_TM_->heartbeat & ThreadPropertiesList[i]->BitID)
		{
			// if high put ith bit (flag down)
			SM_TM_->heartbeat ^= ThreadPropertiesList[i]->BitID;
			// Reset the stopwatch
			SM_TM_->WatchList[i]->Restart();
		}
		else
		{
			// check the stopwatch. Is time exceeded crash time limit?
			if (SM_TM_->WatchList[i]->ElapsedMilliseconds > CRASH_LIMIT)
			{
				// is ith process a critical process?
				if (ThreadPropertiesList[i]->Critical)
				{
					// shutdown all
					Console::WriteLine(ThreadPropertiesList[i]->ThreadName + " failure. Shutting down all threads.");
					shutdownModules();
					return ERR_CRITICAL_PROCESS_FAILURE;
				}
				else
				{
					// try to restart
					Console::WriteLine(ThreadPropertiesList[i]->ThreadName + " failed. Attempting to restart.");
					ThreadList[i]->Abort();
					ThreadList[i] = gcnew Thread(ThreadPropertiesList[i]->ThreadStart_);
					SM_TM_->ThreadBarrier = gcnew Barrier(1);
					ThreadList[i]->Start();
				}

			}

		}
	}
	return SUCCESS;
}