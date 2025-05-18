#using <System.dll>
#include "Laser.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

Laser::Laser(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser)
{
	SM_Laser_ = SM_Laser;
	SM_TM_ = SM_TM;
	Watch = gcnew Stopwatch;
}

void Laser::threadFunction()
{
	//Console::WriteLine("Laser thread is starting");
	// setup the stopwatch
	Watch = gcnew Stopwatch;

	// setup the Laser
	int maxAttempts = 10;
	while (maxAttempts > 0)
	{
		if (connect(WEEDER_ADDRESS, 23000) == SUCCESS)
		{
			//Console::WriteLine("Connected to Laser!");
			break;
		}
		else
		{
			//Console::WriteLine("Unable to connect with Laser. Retrying...");
			maxAttempts--;
		}
	}

	if (maxAttempts == 0)
	{
		//Console::WriteLine("Max attempts at connecting with Laser. Shutting down all modules...");
		shutdownModules();
	}

	// wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();

	// Start the stopwatch
	Watch->Start();

	// Start thread loop
	while (!getShutdownFlag())
	{
		//Console::WriteLine("Laser thread is running.");
		processHeartbeats();
		
		int maxAttempts = 5;
		while (maxAttempts > 0)
		{
			if (communicate() == SUCCESS && processSharedMemory() == SUCCESS)
			{
				//Console::WriteLine("Communicated with Laser!");
				break;
			}
			else
			{
				if (communicate() != SUCCESS)
				{
					//Console::WriteLine("Unable to communicate with laser. Retrying...");
				}
				else
				{
					//Console::WriteLine("Unable to process shared memory of laser. Retrying...");
				}
				
				maxAttempts--;
			}
		}

		if (maxAttempts == 0)
		{
			//Console::WriteLine("Max attempts at communicating with Laser. Shutting down all modules...");
			shutdownModules();
		}

		Thread::Sleep(20);
	}
	Console::WriteLine("Laser thread is terminating");
}

error_state Laser::connect(String^ hostName, int portNumber)
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

error_state Laser::communicate()
{
	// Sends Command
	String^ Command = "sRN LMDscandata";
	SendData = System::Text::Encoding::ASCII->GetBytes(Command);
	Stream->WriteByte(0x02); // sending STX
	Stream->Write(SendData, 0, SendData->Length);
	Stream->WriteByte(0x03); // sending ETX

	System::Threading::Thread::Sleep(10);

	return SUCCESS;
}

error_state Laser::processSharedMemory()
{
	Stream->Read(ReadData, 0, ReadData->Length);
	String^ ResponseData = System::Text::Encoding::ASCII->GetString(ReadData);

	array<String^>^ Fragments = ResponseData->Split(' ');

	//Console::WriteLine(Fragments->Length);

	double Resolution = System::Convert::ToInt32(Fragments[24], 16) / 10000.0;
	int NumPoints = System::Convert::ToInt32(Fragments[25], 16);

	array<double>^ Range = gcnew array<double>(NumPoints);

	//Console::WriteLine(NumPoints);

	if (NumPoints != 361)
	{
		return ERR_INVALID_DATA;
	}

	Monitor::Enter(SM_Laser_->lockObject);

	for (int i = 0; i < NumPoints; i++)
	{
		Range[i] = System::Convert::ToInt32(Fragments[26 + i], 16);
		//SM_Laser_->x[i] = Range[i] * Math::Sin(i * Resolution * Math::PI / 180.0);
		//SM_Laser_->y[i] = -1 * Range[i] * Math::Cos(i *  Resolution * Math::PI / 180.0);

		SM_Laser_->x[i] = Range[i] * Math::Cos(i * Resolution * Math::PI / 180.0);
		SM_Laser_->y[i] = Range[i] * Math::Sin(i * Resolution * Math::PI / 180.0);
		//Console::WriteLine("X value: " + SM_Laser_->x[i] + " Y value: " + SM_Laser_->y[i]);
	}

	Monitor::Exit(SM_Laser_->lockObject);

	return SUCCESS;

}

void Laser::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool Laser::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_LASER;
}

error_state Laser::processHeartbeats()
{
	if ((SM_TM_->heartbeat & bit_LASER) == 0)
	{
		SM_TM_->heartbeat |= bit_LASER;
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