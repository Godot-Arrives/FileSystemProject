#include <string.h>

#define PORITON_FREE 0xFF
#define PORTION_ALLOC = 0x0F
#define PORTION_USED = 0x00

#define EOF -1

// **************************************** User API ****************************************

// In progress
CSC322FILE *CSC322_fopen(const char *filename,
		      const char *mode)
{
	if(findOpenFile(filename) != NULL)
		return NULL;
	
	FILEHEADER searchHeader;
	UINT32 searchLocation = 0;
	bool found = false;

	while(thisSector(searchLocation) < 19)
	{
		readBuffer(&searchHeader,
			   searchLocation,
			   sizeof(FILEHEADER));

		if(strcmp(filename, searchHeader.filename) == 0 && searchHeader.portionType == 0x0F)
		{
			found = true;
			break;
		}
	       	else if(searchHeader.portionType == 0xFF && thisSector(searchLocation) == 19)
		{
			break;
		}
		else if(searchHeader.portionType == 0xFF)
		{
			searchLocation = (1 + thisSector(searchLocation))*nSectorSizeBytes;
			continue;
		}

		searchLocation += sizeof(FILEHEADER) + searchHeader.fileSizeBytes;

	}

	CSC322FILE *requested = createFile(filename,
					   mode);

	if(found)
	{
		requested->headerExists = 1;
		requested->headerLocation = searchLocation;
		requested->filesize = searchHeader.fileSizeBytes;
		requested->modified = 0;

	}
	else
	{
		requested->headerExists = 0;
		requested->filesize = 0;
		requested->modified = 0;
		requested->inMemoryFile = NULL;
		requested->filepointer = NULL;
		return requested;
	}

	requested->inMemoryFile = HeapAlloc(GetProcessHeap(),
					    HEAP_ZERO_MEMORY,
					    requested->filesize);


	readBuffer(requested->inMemoryFile,
		   requested->headerLocation + sizeof(FILEHEADER),
		   requested->filesize);

	switch(requested->type)
	{
		case wb:
		case wpb:
		case rb: requested->filepointer = requested->inMemoryFile; break;
		case ab: requested->filepointer = (char *)requested->inMemoryFile + requested->filesize - 1; break;
		default: requested->filepointer = NULL; break;
	}
	
	return requested;
}

// Complete, not tested. Check on being passed old pointers.
int CSC322_fclose(CSC322FILE *stream)
{
	if(stream == NULL) 
		return EOF;

	if(!stream->modified)
	{
		closeNode(stream->filename);
		return 0;
	}

	FILEHEADER searchHeader;
	UINT32 searchLocation;
	bool found = false, garbageCollected = false;

	while(!found)
	{
		searchLocation = 0;

		while(thisSector(searchLocation) < 19)
		{
			readBuffer(&searchHeader,
				   searchLocation,
				   sizeof(FILEHEADER));

			if(searchHeader.portionType == 0xFF && (thisSector(searchLocation) + 1)*nSectorSizeBytes - (searchLocation + sizeof(FILEHEADER)) >= stream->filesize)
			{
				found = true;
				break;
			}
			else if(searchHeader.portionType == 0xFF)
			{
				searchLocation = (1 + thisSector(searchLocation))*nSectorSizeBytes;
				continue;
			}

			searchLocation += sizeof(FILEHEADER) + searchHeader.fileSizeBytes;

		}

		if(!found && garbageCollected)
		{
			break;
		}
		if(!found)
		{
			garbageCollect();
			garbageCollected = true;
		}
	}

	if(!found)
		return EOF;

	FILEHEADER usedHeader;

	if(stream->headerExists)
	{
		readBuffer(&usedHeader,
			   stream->headerLocation,
			   sizeof(FILEHEADER));

		usedHeader.portionType = 0x00;

		writeBuffer(&usedHeader,
			    stream->headerLocation,
			    sizeof(FILEHEADER));
	}


	strcpy(searchHeader.filename, stream->filename);

	searchHeader.portionType = 0x0F;
	searchHeader.fileSizeBytes = stream->filesize;

	writeBuffer(&searchHeader,
		    searchLocation,
		    sizeof(FILEHEADER));

	writeBuffer(stream->inMemoryFile,
		    searchLocation + sizeof(FILEHEADER),
		    stream->filesize);

	closeNode(stream->filename);

	return 0;
}

// Complete, not tested.
int CSC322_fread(LPVOID buffer,
		 int size,
		 int count,
		 CSC322FILE *stream)
{
	if(buffer == NULL || (char *)stream->filepointer > (char *)stream->inMemoryFile + stream->filesize - 1 || stream->type == wb || stream->type == ab)
		return 0;

	int readLength = size * count;
	int maxCount = ((char *)stream->inMemoryFile + stream->filesize - (char *)stream->filepointer)/size;
	int readCount = (maxCount > count) ? count : maxCount;

	CopyMemory(buffer,
		   stream->filepointer,
		   size*readCount);

	stream->filepointer = (char *)stream->filepointer + readLength;

	return readCount;
}

// Complete, not tested.
int CSC322_fwrite(LPVOID buffer,
		int size,
		int count,
		CSC322FILE *stream)
{
	if(stream == NULL || stream->type == rb)
	       return 0;

	stream->modified = true;
	int writeLength = size * count;
	int newsize = (char *)stream->filepointer + writeLength - (char *)stream->inMemoryFile;

	if(newsize > stream->filesize)
	{
		LPVOID pBiggerFile = HeapAlloc(GetProcessHeap(),
					       HEAP_ZERO_MEMORY,
					       newsize);

		if(stream->filesize > 0)
		{
			CopyMemory(pBiggerFile,
				   stream->inMemoryFile,
				   stream->filesize);
		}

		LPVOID newFilePointer = (char *)pBiggerFile + ((char *)stream->filepointer - (char *)stream->inMemoryFile);

		CopyMemory(newFilePointer,
			   buffer,
			   writeLength);

		if(stream->filesize > 0)
		{
			HeapFree(GetProcessHeap(),
				 0,
				 stream->inMemoryFile);
		}

		stream->inMemoryFile = pBiggerFile;
		stream->filepointer = (char *)newFilePointer + writeLength;
		stream->filesize = newsize;
	}
	else
	{
		CopyMemory(stream->filepointer,
			   buffer,
			   writeLength);
	}

	return count;
}

// Complete, not tested.
int CSC322_fseek(CSC322FILE *stream,
	  long offset,
	  int origin)
{
	if(origin == SEEK_SET)
	{
		if(offset >= stream->filesize || offset < 0)
			return 1;

		stream->filepointer = (char *)stream->inMemoryFile + offset;

		return 0;
	}
	else if(origin == SEEK_END)
	{
		if(offset > 0 || stream->filesize + offset < 0)
			return 1;

		stream->filepointer = (char *)stream->inMemoryFile + (stream->filesize - offset);

		return 0;
	}
	else if(origin == SEEK_CUR)
	{
		if((char *)stream->filepointer + offset > (char *)stream->inMemoryFile + stream->filesize || (char *)stream->filepointer + offset < (char *)stream->inMemoryFile)
			return 1;

		stream->filepointer = (char *)stream->filepointer + offset;

		return 0;
	}
	else
		return 2;
}

// Complete, not tested.
long CSC322_ftell(CSC322FILE *stream)
{
	return long((char *)stream->filepointer - (char *)stream->inMemoryFile);
}

// In progress.
int CSC322_remove(const char *filename)
{
	if(findOpenFile(filename) != NULL)
		return -1;

	FILEHEADER searchHeader;
	UINT32 searchLocation = 0;
	bool found = false;

	while(thisSector(searchLocation) < 19)
	{
		readBuffer(&searchHeader,
			   searchLocation,
			   sizeof(FILEHEADER));

		if(strcmp(filename, searchHeader.filename) == 0 && searchHeader.portionType == 0x0F)
		{
			found = true;
			break;
		}
	       	else if(searchHeader.portionType == 0xFF && thisSector(searchLocation) == 19)
		{
			break;
		}
		else if(searchHeader.portionType == 0xFF)
		{
			searchLocation = (1 + thisSector(searchLocation))*nSectorSizeBytes;
			continue;
		}

		searchLocation += sizeof(FILEHEADER) + searchHeader.fileSizeBytes;

	}

	if(!found)
		return -1;

	searchHeader.portionType = 0x00;

	writeBuffer(&searchHeader,
		    searchLocation,
		    sizeof(FILEHEADER));

	return 0;
}

// *********************************** Service Functions *************************************


// Complete, not tested.
CSC322FILE* findOpenFile(const char *filename)
{
	FNODE *search = head;

	while(search != NULL && strcmp(search->file.filename, filename))
		search = search->next;

	return &(search->file);
}

// Complete, not tested.
CSC322FILE* createFile(const char *filename,
	       	     const char *mode)
{
	FNODE newFNODE;
	
	strcpy(newFNODE.file.filename, filename);
	newFNODE.next = NULL;
	
	if(!strcmp(mode, "ab"))
		newFNODE.file.type = ab;
	else if(!strcmp(mode, "wb"))
		newFNODE.file.type = wb;
	else if(!strcmp(mode, "rb"))
		newFNODE.file.type = rb;
	else if(!strcmp(mode, "w+b"))
		newFNODE.file.type = wpb;
	else
		return NULL;

	FNODE *pFNODE = (FNODE *)HeapAlloc(GetProcessHeap(),
					     HEAP_ZERO_MEMORY,
					     sizeof(FNODE));

	*pFNODE = newFNODE;

	if(head == NULL)
		head = pFNODE;
	else
	{
		FNODE *search = head;

		while(search->next != NULL)
			search = search->next;

		search->next = pFNODE;
	}
	
	return &(pFNODE->file);
}

// Complete, not tested
void closeNode(const char *filename)
{
	FNODE *pSearch = head, *pPrev = head;

	while(pSearch != NULL && strcmp(pSearch->file.filename, filename))
	{
		pSearch = pSearch->next;
		pPrev = pSearch;
	}

	if(pSearch == NULL)
		return;

	if(pSearch == pPrev)
	{
		head = head->next;
	}
	else
	{
		pPrev->next = pSearch->next;
	}

	HeapFree(GetProcessHeap(),
		 0,
		 pSearch);

	return;
}

// Complete, tested. 
void readBuffer(LPVOID buffer,
		UINT32 location,
		int lengthBytes)
{
	int word = 0;
	UINT16 data;

	// Misaligned first byte.
	if(location % 2 == 1)
	{
		*(UINT8 *)buffer = ReadWord(location - 1);
		word++;
	}

	// All aligned words.
	for(word; (word + 1)*2 - location%2 <= lengthBytes; word++)
	{
		data = ReadWord(location - location%2 + word*2);
		*((char *)buffer + word*2 - location%2) = *(char *)&data; 
		*((char *)buffer + word*2 - location%2 + 1) = *((char *)&data + 1);
	}
	
	// Misaligned last byte.
	if((location + lengthBytes) % 2 == 1)
	{
		*(UINT8 *)((char *)buffer + word*2 + 1) = (ReadWord(location + word*2 + 2) >> 8);
	}
}

// Complete, tested.
void writeBuffer(LPVOID buffer,
		 UINT32 location,
		 int lengthBytes)
{
	int word = 0;
	UINT16 data;

	// Misaligned first byte.
	if(location % 2 == 1)
	{
		data = ReadWord(location - 1);
		data = (data & 0xFF00) | (*(UINT8 *)buffer);
		WriteWord(location - 1, data);
		word++;
	}

	// All aligned words.
	for(word; (word + 1)*2 - location%2 <= lengthBytes; word++)
	{
		data = *( (UINT16 *)buffer + word);
		// data = ((UINT16)(*((char *)buffer + word*2)) << 8) | *((char *)buffer + word*2 + 1);
		WriteWord(location - location%2 + word*2, data);
	}

	// Misaligned last byte.
	if((location + lengthBytes) % 2 == 1)
	{
		data = ReadWord(location - location%2 + word*2);
		data = (data & 0xFF00) | ((UINT8)(*((char *)buffer + word*2 + 1))) ;
		WriteWord(location - location%2 + word*2, data);
	}
}

// Complete, tested.
int thisSector(int nLocationBytes)
{
	return nLocationBytes / nSectorSizeBytes;
}

void garbageCollect()
{
	FNODE *wp = head;
	FILEHEADER killFile;

	while(wp != NULL)
	{
		if(wp->file.headerExists)
		{
			readBuffer(&killFile,
				   wp->file.headerLocation,
				   sizeof(FILEHEADER));

			killFile.portionType = 0x00;

			writeBuffer(&killFile,
				    wp->file.headerLocation,
				    sizeof(FILEHEADER));
		}

		wp->file.headerExists = false;
		wp->file.modified = true;

		wp = wp->next;
	}

	FILEHEADER collectionHeader;

	int searchLocation = 0, 
	    writeBackLocation = 0, 
	    tempLocation = 19*nSectorSizeBytes, 
	    tempWriteBack = 19*nSectorSizeBytes,
	    opSector = 0;

	LPVOID tempFileHolder;

	while(thisSector(searchLocation) < 19)
	{
		while(thisSector(searchLocation) == opSector)
		{
			readBuffer(&collectionHeader,
				   searchLocation,
			   	   sizeof(FILEHEADER));	

			if(collectionHeader.portionType == 0x0F)
			{
				tempFileHolder = HeapAlloc(GetProcessHeap(),
							   HEAP_ZERO_MEMORY,
							   collectionHeader.fileSizeBytes + sizeof(FILEHEADER));

				readBuffer(tempFileHolder,
					   searchLocation,
					   collectionHeader.fileSizeBytes + sizeof(FILEHEADER));

				writeBuffer(tempFileHolder,
					    tempLocation,
					    collectionHeader.fileSizeBytes + sizeof(FILEHEADER));

				HeapFree(GetProcessHeap(),
					 0,
					 tempFileHolder);

				tempLocation += collectionHeader.fileSizeBytes + sizeof(FILEHEADER);

				searchLocation += collectionHeader.fileSizeBytes + sizeof(FILEHEADER);
			}
			else if(collectionHeader.portionType == 0xFF)
			{
				searchLocation = (thisSector(searchLocation) + 1)*nSectorSizeBytes;
			}
			else
			{
				searchLocation += collectionHeader.fileSizeBytes + sizeof(FILEHEADER);
			}
		}

		EraseSector(opSector);

		readBuffer(&collectionHeader,
			   tempWriteBack,
			   sizeof(FILEHEADER));

		while(collectionHeader.portionType == 0x0F)
		{
			tempFileHolder = HeapAlloc(GetProcessHeap(),
						   HEAP_ZERO_MEMORY,
						   collectionHeader.fileSizeBytes + sizeof(FILEHEADER));

			readBuffer(tempFileHolder,
				   tempWriteBack,
				   collectionHeader.fileSizeBytes + sizeof(FILEHEADER));

			writeBuffer(tempFileHolder,
				    writeBackLocation,
				    collectionHeader.fileSizeBytes + sizeof(FILEHEADER));

			HeapFree(GetProcessHeap(),
				 0,
				 tempFileHolder);

			tempWriteBack += collectionHeader.fileSizeBytes + sizeof(FILEHEADER);
			writeBackLocation += collectionHeader.fileSizeBytes + sizeof(FILEHEADER);

			readBuffer(&collectionHeader,
				   tempWriteBack,
			  	   sizeof(FILEHEADER));
		}

		EraseSector(19);
		tempLocation = 19*nSectorSizeBytes;
	  	tempWriteBack = 19*nSectorSizeBytes;
		opSector++;
		writeBackLocation = opSector*nSectorSizeBytes;
	}
}
