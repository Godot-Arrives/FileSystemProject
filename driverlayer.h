// Authors: Jacob Gatlin, Patrick Luckett, Joel Dawson, Jan Hellmich
#include <windows.h>

#define nFileSizeBytes 1310720
#define nSectorSizeBytes 65536

enum layerErrors
{
	noError,
	invalidAddress,
	fileNotFound

} layerError = noError;


void EraseAllSectors()
{
	DWORD dwBytesWritten = 0;

	// CreateFile is the recommended way of getting file handles, it doesn't actually create the file every time unless you tell it to.
	HANDLE memory = CreateFile( "memory.bin",
				(GENERIC_READ | GENERIC_WRITE),
				0,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
	
	layerError = (GetLastError() == 0) ? fileNotFound : noError;

	// We allocate a sector's worth of memory off the heap and fill it with ones.
	LPVOID freshMemorySector = HeapAlloc(GetProcessHeap(),
					HEAP_GENERATE_EXCEPTIONS,
					nFileSizeBytes/20);

	FillMemory(freshMemorySector,
		nFileSizeBytes/20,
		0xff);

	// We write that block of memory 20 times.
	for(int sect = 0; sect < 20; sect++){
	
		WriteFile( memory,
			freshMemorySector,
			nFileSizeBytes/20,
			&dwBytesWritten,
			NULL);
	}

	HeapFree(GetProcessHeap(),
		0,
		freshMemorySector);

	CloseHandle(memory);
}

void EraseSector(int nSectorNr)
{
	if(nSectorNr > 19 || nSectorNr < 0)
	{
		layerError = invalidAddress;
		return;
	}

	DWORD dwBytesWritten = 0;

	HANDLE memory = CreateFile(  "memory.bin",
				(GENERIC_READ | GENERIC_WRITE),
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

	if(GetLastError() == ERROR_FILE_NOT_FOUND) 
	{
		EraseAllSectors();

		memory = CreateFile(  "memory.bin",
				(GENERIC_READ | GENERIC_WRITE),
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
	}

	// We allocate a sector's worth of memory and fill it with ones.
	LPVOID freshMemorySector = HeapAlloc(GetProcessHeap(),
					HEAP_GENERATE_EXCEPTIONS,
					nFileSizeBytes/20);

	FillMemory(freshMemorySector,
		nFileSizeBytes/20,
		0xFF);
	
	// We write that block to the specified sector.
	SetFilePointer(memory,
			nSectorNr * nFileSizeBytes/20,
			NULL,
			FILE_BEGIN);

	WriteFile( memory,
		freshMemorySector,
		nFileSizeBytes/20,
		&dwBytesWritten,
		NULL);

	HeapFree(GetProcessHeap(),
		0,
		freshMemorySector);

	CloseHandle(memory);
	
}

UINT16 ReadWord(int nAddress)
{
	if(nAddress < 0 || nAddress > nFileSizeBytes || nAddress % 2 != 0)
	{
		layerError = invalidAddress;
		return 0x0000;
	}

	DWORD dwBytesRead = 0;
		
	HANDLE memory = CreateFile(  "memory.bin",
				(GENERIC_READ | GENERIC_WRITE),
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

	if(GetLastError() == ERROR_FILE_NOT_FOUND) 
	{
		EraseAllSectors();

		memory = CreateFile(  "memory.bin",
				(GENERIC_READ | GENERIC_WRITE),
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
	}

	SetFilePointer(memory,
			nAddress,
			NULL,
			FILE_BEGIN);
	
	UINT16 dReadValue;

	ReadFile(memory,
		&dReadValue,
		2,
		&dwBytesRead,
		NULL);

	CloseHandle(memory);
	
	return dReadValue;
}

void WriteWord(int nAddress, UINT16 nWord)
{
	if(nAddress < 0 || nAddress > nFileSizeBytes || nAddress % 2 != 0)
	{
		layerError = invalidAddress;
		return;
	}

	DWORD dwBytesWritten = 0;

	// As specified, stored value is (currentValue & writeValue).
	nWord = nWord & ReadWord(nAddress);

	HANDLE memory = CreateFile( "memory.bin",
				(GENERIC_READ | GENERIC_WRITE),
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

	// We don't need to test if the file exists because we just called ReadWord, which does.

	SetFilePointer(memory,
			nAddress,
			NULL,
			FILE_BEGIN);

	WriteFile(memory,
		&nWord,
		2,
		&dwBytesWritten,
		NULL);



	CloseHandle(memory);
	
}
