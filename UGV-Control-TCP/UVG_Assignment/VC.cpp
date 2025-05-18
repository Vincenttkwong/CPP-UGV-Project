#using <System.dll>
#include "VC.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

VehicleControl::VehicleControl(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl)
{
	SM_VehicleControl_ = SM_VehicleControl;
	SM_TM_ = SM_TM;
	Watch = gcnew Stopwatch;
}

void VehicleControl::threadFunction()
{
	//Console::WriteLine("VC thread is starting");
	// setup the stopwatch
	Watch = gcnew Stopwatch;

	// setup the Laser
	int maxAttempts = 5;
	while (maxAttempts > 0)
	{
		if (connect(WEEDER_ADDRESS, 25000) == SUCCESS)
		{
			//Console::WriteLine("Connected to Vehicle Control!");
			break;
		}
		else
		{
			//Console::WriteLine("Unable to connect with Vehicle Control. Retrying...");
			maxAttempts--;
		}
	}

	if (maxAttempts == 0)
	{
		//Console::WriteLine("Max attempts at connecting with Vehicle Control. Shutting down all modules...");
		shutdownModules();
	}

	// wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();

	// Start the stopwatch
	Watch->Start();

	// Start thread loop
	while (!getShutdownFlag())
	{
		//Console::WriteLine("VC thread is running.");
		processHeartbeat();

		int maxAttempts = 5;
		while (maxAttempts > 0)
		{
			if (communicate() == SUCCESS && processSharedMemory() == SUCCESS)
			{
				//Console::WriteLine("Communicated with Vehicle Control!");
				break;
			}
			else
			{
				if (communicate() != SUCCESS)
				{
					//Console::WriteLine("Unable to communicate with Vehicle Control. Retrying...");
				}
				else
				{
					//Console::WriteLine("Unable to process shared memory of Vehicle Control. Retrying...");
				}

				maxAttempts--;
			}
		}

		if (maxAttempts == 0)
		{
			//Console::WriteLine("Max attempts at communicating with Vehicle Control. Shutting down all modules...");
			shutdownModules();
		}

		Thread::Sleep(20);
	}
	Console::WriteLine("VC thread is terminating");
}

error_state VehicleControl::connect(String^ hostName, int portNumber)
{
	Client = gcnew TcpClient(hostName, portNumber);
	Stream = Client->GetStream();

	Client->NoDelay = true;
	Client->ReceiveTimeout = 500;
	Client->SendTimeout = 500;
	Client->ReceiveBufferSize = 1024;
	Client->SendBufferSize = 1024;

	SendData = gcnew array<unsigned char>(64);
	ReadData = gcnew array<unsigned char>(2048);

	//Auth
	SendData = System::Text::Encoding::ASCII->GetBytes("5312783\n");
	Stream->Write(SendData, 0, SendData->Length);

	Stream->Read(ReadData, 0, ReadData->Length);
	String^ ResponseData = System::Text::Encoding::ASCII->GetString(ReadData);

	if (ResponseData->Substring(0, 3) != "OK\n")
	{
		Console::WriteLine("NOT AUTHENTICATED");
		return ERR_CONNECTION;
	}
	Console::WriteLine("Authenticated");

	return SUCCESS;
}

error_state VehicleControl::communicate()
{
	Monitor::Enter(SM_VehicleControl_->lockObject);
	flag = 1 - flag;
	String^ Command = String::Format("# {0} {1} {2} #", SM_VehicleControl_->Steering, SM_VehicleControl_->Speed, flag);
	Monitor::Exit(SM_VehicleControl_->lockObject);

	//Console::WriteLine(Command);

	SendData = System::Text::Encoding::ASCII->GetBytes(Command);
	Stream->Write(SendData, 0, SendData->Length);
	System::Threading::Thread::Sleep(10);

	return SUCCESS;
}

error_state VehicleControl::processSharedMemory()
{
	// Check if Steering is between +- 40 and Check if Speed is between +- 1
	if ((SM_VehicleControl_->Steering > 40 || SM_VehicleControl_->Steering < -40) ||
		(SM_VehicleControl_->Speed > 1 || SM_VehicleControl_->Speed < -1)) {
		return ERR_INVALID_DATA;
	}
	return SUCCESS;
}

void VehicleControl::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool VehicleControl::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_VC;
}

error_state VehicleControl::processHeartbeat()
{
	if ((SM_TM_->heartbeat & bit_VC) == 0)
	{
		SM_TM_->heartbeat |= bit_VC;
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