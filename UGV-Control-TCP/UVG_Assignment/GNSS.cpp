#using <System.dll>
#include "GNSS.h"

#pragma pack(1)

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

struct GNSS
{
	unsigned int Header; // 4 bytes 0xAA 0x44 0x12 0x1C;
	unsigned char Discards1[40];
	double Northing;
	double Easting;
	double Height;
	unsigned char Discards2[40];
	unsigned int CRC;
} GNSS_Struct;

GPS::GPS(SM_ThreadManagement^ SM_TM, SM_GPS^ SM_GPS)
{
	SM_GPS_ = SM_GPS;
	SM_TM_ = SM_TM;
	Watch = gcnew Stopwatch;
}

void GPS::threadFunction()
{
	//Console::WriteLine("GNSS thread is starting");
	// setup the stopwatch
	Watch = gcnew Stopwatch;

	// setup the GNSS - non-critical can try as many times as needed
	connect(WEEDER_ADDRESS, 24000);

	// wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();

	// Start the stopwatch
	Watch->Start();
	
	// Start thread loop
	while (!getShutdownFlag())
	{
		//Console::WriteLine("GNSS thread is running.");
		processHeartbeat();

		// non-critical thread, can try as many times as needed
		communicate();
		processSharedMemory();

		Thread::Sleep(20);
	}
	//Console::WriteLine("GNSS thread is terminating");
}

error_state GPS::connect(String^ hostName, int portNumber)
{
	Client = gcnew TcpClient(hostName, portNumber);
	Stream = Client->GetStream();

	Client->NoDelay = true;
	Client->ReceiveTimeout = 500;
	Client->SendTimeout = 500;
	Client->ReceiveBufferSize = 1024;
	Client->SendBufferSize = 1024;

	SendData = gcnew array<unsigned char>(64);
	ReadData = gcnew array<unsigned char>(5000);

	return SUCCESS;
}

error_state GPS::communicate()
{
	
	unsigned int Header = 0;
	unsigned char Data;

	if (Stream->DataAvailable)
	{
		do
		{
			Data = Stream->ReadByte();
			Header = ((Header << 8) | Data);
		} while (Header != 0xaa44121c);

		ReadData[0] = 0xAA;
		ReadData[1] = 0x44;
		ReadData[2] = 0x12;
		ReadData[3] = 0x1c;

		Stream->Read(ReadData, 4, ReadData->Length - 4);

	}

	//Console::WriteLine("Size of Struct is {0:D}", sizeof(GNSS_Struct));

	unsigned char* BytePtr = (unsigned char*)&GNSS_Struct;
	for (int i = 0; i < sizeof(GNSS_Struct); i++)
	{
		*(BytePtr++) = ReadData[i];
	}

	BytePtr = (unsigned char*)&GNSS_Struct;
	unsigned long calculatedCRC = CalculateBlockCRC32(108, BytePtr);
	unsigned long weederCRC = GNSS_Struct.CRC;

	if (calculatedCRC != weederCRC) {
		return ERR_INVALID_DATA;
	}

	Monitor::Enter(SM_GPS_->lockObject);
	SM_GPS_->Easting = GNSS_Struct.Easting;
	SM_GPS_->Northing = GNSS_Struct.Northing;
	SM_GPS_->Height = GNSS_Struct.Height;
	Monitor::Exit(SM_GPS_->lockObject);

	Console::Write("Northing: {0:F3} Easting: {1:F3} Height: {2:F3} ", SM_GPS_->Northing, SM_GPS_->Easting, SM_GPS_->Height);
	Console::Write(" Calc CRC: " + calculatedCRC);
	Console::WriteLine(" weed CRC: " + weederCRC);


	return SUCCESS;
}

error_state GPS::processSharedMemory()
{
	return SUCCESS;
}

void GPS::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool GPS::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_GPS;
}

error_state GPS::processHeartbeat()
{
	if ((SM_TM_->heartbeat & bit_GPS) == 0)
	{
		SM_TM_->heartbeat |= bit_GPS;
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

unsigned long GPS::CRC32Value(int i)
{
	int j;
	unsigned long ulCRC;
	ulCRC = i;
	for (j = 8; j > 0; j--)
	{
		if (ulCRC & 1)
			ulCRC = (ulCRC >> 1) ^ CRC32_POLYNOMIAL;
		else
			ulCRC >>= 1;
	}
	return ulCRC;
}

unsigned long GPS::CalculateBlockCRC32(
	unsigned long ulCount, /* Number of bytes in the data block */
	unsigned char* ucBuffer) /* Data block */
{
	unsigned long ulTemp1;
	unsigned long ulTemp2;
	unsigned long ulCRC = 0;
	while (ulCount-- != 0)
	{
		ulTemp1 = (ulCRC >> 8) & 0x00FFFFFFL;
		ulTemp2 = CRC32Value(((int)ulCRC ^ *ucBuffer++) & 0xff);
		ulCRC = ulTemp1 ^ ulTemp2;
	}
	return(ulCRC);
}