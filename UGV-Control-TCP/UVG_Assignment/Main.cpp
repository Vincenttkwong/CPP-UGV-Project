#include "TMM.h"

using namespace System;

int main() {

	ThreadManagement^ myTMM = gcnew ThreadManagement();

	myTMM->setupSharedMemory();

	myTMM->threadFunction();

	Console::ReadKey();

	return 0;
}