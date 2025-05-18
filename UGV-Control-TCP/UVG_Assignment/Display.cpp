#using <System.dll>
#include "Display.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

Display::Display(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser)
{
	SM_TM_ = SM_TM;
	SM_Laser_ = SM_Laser;
	Watch = gcnew Stopwatch;
}

void Display::threadFunction()
{
	//Console::WriteLine("Display thread is starting");
	// setup the stopwatch
	Watch = gcnew Stopwatch;

	// setup the Display
	int maxAttempts = 5;
	while (maxAttempts > 0)
	{
		if (connect(DISPLAY_ADDRESS, 28000) == SUCCESS)
		{
			//Console::WriteLine("Connected to Display!");
			break;
		}
		else
		{
			//Console::WriteLine("Unable to connect with Display. Retrying...");
			maxAttempts--;
		}
	}

	if (maxAttempts == 0)
	{
		//Console::WriteLine("Max attempts at connecting with Display. Shutting down all modules...");
		shutdownModules();
	}
	
	// wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();

	// Start the stopwatch
	Watch->Start();

	// Start thread loop
	while (!getShutdownFlag())
	{
		//Console::WriteLine("Display thread is running.");
		processHeartbeat();

		int maxAttempts = 5;
		while (maxAttempts > 0)
		{
			if (communicate() == SUCCESS)
			{
				//Console::WriteLine("Communicated with Display!");
				break;
			}
			else
			{
				//Console::WriteLine("Unable to communicate with Display. Retrying...");
				maxAttempts--;
			}
		}

		if (maxAttempts == 0)
		{
			//Console::WriteLine("Max attempts at communicating with Display. Shutting down all modules...");
			shutdownModules();
		}

		Thread::Sleep(20);
	}
	Console::WriteLine("Display thread is terminating");
}

error_state Display::connect(String^ hostName, int portNumber)
{
	Client = gcnew TcpClient(hostName, portNumber);
	Stream = Client->GetStream();
	Client->NoDelay = true;
	Client->ReceiveTimeout = 500;
	Client->SendTimeout = 500;
	Client->ReceiveBufferSize = 1024;
	Client->SendBufferSize = 1024;

	SendData = gcnew array<unsigned char>(64);
	ReadData = gcnew array<unsigned char>(64); // Change size later
	return SUCCESS;
}

error_state Display::communicate()
{
	sendDisplayData(SM_Laser_->x, SM_Laser_->y, Stream);
	return SUCCESS;
}

error_state Display::processSharedMemory()
{
	return SUCCESS;
}

void Display::sendDisplayData(array<double>^ xData, array<double>^ yData,
	NetworkStream^ stream) 
{
	// Serialize the data arrays to a byte array
	//(format required for sending)
	Monitor::Enter(SM_Laser_->lockObject);
	array<Byte>^ dataX =
		gcnew array<Byte>(SM_Laser_->x->Length * sizeof(double));
	Buffer::BlockCopy(SM_Laser_->x, 0, dataX, 0, dataX->Length);
	array<Byte>^ dataY =
		gcnew array<Byte>(SM_Laser_->y->Length * sizeof(double));
	Buffer::BlockCopy(SM_Laser_->y, 0, dataY, 0, dataY->Length);
	Monitor::Exit(SM_Laser_->lockObject);

	// Send byte data over connection
	Stream->Write(dataX, 0, dataX->Length);
	Thread::Sleep(10);
	Stream->Write(dataY, 0, dataY->Length);
}

void Display::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool Display::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_GPS;
}

error_state Display::processHeartbeat()
{
	if ((SM_TM_->heartbeat & bit_DISPLAY) == 0)
	{
		SM_TM_->heartbeat |= bit_DISPLAY;
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