#include "driverlayer.h"

enum accessType
{
	rb,
	wb,
	ab,
	wpb
};

typedef struct FILEHEADER
{
	char filename[64];
	UINT8 portionType;
	UINT16 fileSizeBytes;
} FILEHEADER;

#define HEADER_SIZE_BYTES 96

typedef struct CSC322FILE
{
	char filename[64];
	LPVOID filepointer;
	accessType type;
	int headerLocation;
	int filesize;	
	LPVOID inMemoryFile;
	bool modified;
	bool headerExists;
} CSC322FILE;

typedef struct FNODE
{
	CSC322FILE file;
	FNODE *next;
} FNODE;

FNODE *head = NULL;

// **************************************** User API ****************************************

CSC322FILE *CSC322_fopen(const char *filename,
		      const char *mode);

int CSC322_fclose(CSC322FILE *stream);

int CSC322_fread(LPVOID buffer,
		 int size,
		 int count,
		 CSC322FILE *stream);

int CSC322_fwrite(LPVOID buffer,
		  int size,
		  int count,
		  CSC322FILE *stream);

int CSC322_fseek(CSC322FILE *stream,
		 long offset,
		 int origin);

long CSC322_ftell(CSC322FILE *stream);

int CSC322_remove(const char *filename);

// *********************************** Service Functions *************************************


CSC322FILE* findOpenFile(const char *filename);

CSC322FILE* createFile(const char *filename,
	       	     const char *mode);

void closeNode(const char *filename);

void readBuffer(LPVOID buffer,
		UINT32 location,
		int lengthBytes);

void writeBuffer(LPVOID buffer,
		UINT32 location,
		int lengthBytes);

int thisSector(int nLocationBytes);

void garbageCollect();
