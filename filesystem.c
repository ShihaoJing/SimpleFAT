#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/mman.h>
#include "support.h"
#include "structs.h"
#include "filesystem.h"


#define Kilo  1024
#define Mega (Kilo*Kilo)
#define MAX_FAT_SIZE (128 * Kilo)


/*
 * generateData() - Converts source from hex digits to
 * binary data. Returns allocated pointer to data
 * of size amt/2.
 */
char* generateData(char *source, size_t size)
{
	char *retval = (char *)malloc((size >> 1) * sizeof(char));

	for(size_t i=0; i<(size-1); i+=2)
	{
		sscanf(&source[i], "%2hhx", &retval[i>>1]);
	}
	return retval;
}

void initializeFileSystem(u_int8_t *map) {

}

void verifyFileSystem(u_int8_t *map) {

}

void usage(u_int8_t *map, u_int8_t *root, u_int8_t *FAT) {
  printf("%lu bytes have been used by system\n", root - map);
  int count = 0;
  for (u_int8_t *fp = FAT, *e = fp + MAX_FAT_SIZE; fp != e; fp += 2)
  {
    if (*(u_int16_t*)fp != FREE_CLUSTER)
      ++count;
  }
  printf("%d bytes have been used by actual files\n", count * 512);
}



/*
 * filesystem() - loads in the filesystem and accepts commands
 */
void filesystem(char *file)
{
	/* pointer to the memory-mapped filesystem */
  int volumeSize = 4 * Mega;
  u_int8_t *map = mmap(0, volumeSize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

  BootSector *sysInfo = NULL;
  u_int16_t *FAT = NULL;
  u_int8_t *data = NULL;

  sysInfo = (BootSector*)map;
  sysInfo->BytesPerSector = 512;
  sysInfo->SectorsPerCluster = 1; //cluster size = 4KB
  sysInfo->ReservedSectors = 1;
  sysInfo->FATCopies = 1;
  sysInfo->MaxRootEntries = 512;
  sysInfo->TotalSectors = volumeSize / sysInfo->BytesPerSector;
  sysInfo->SectorsPerFAT = MAX_FAT_SIZE / sysInfo->BytesPerSector;
  memcpy(sysInfo->FileSystemType, "FAT16", 6);

  //initialize FAT
  for (u_int8_t *fp = (u_int8_t*)(sysInfo + 1), *e = fp + MAX_FAT_SIZE; fp != e; fp += 2)
  {
    *(u_int16_t*)fp = FREE_CLUSTER;
  }
  FAT = (u_int16_t*)(sysInfo + 1);
  FAT[0] = RESERVED_CLUSTER; // FAT[0] reserved
  FAT[1] = RESERVED_CLUSTER; // FAT[1] reserved



  FILE_t *root = (FILE_t*)((u_int8_t*)FAT + MAX_FAT_SIZE);
  root->Attr = ATTR_VOLUME_ID; // first entry of root is reserved
  data = (u_int8_t*)(root) + sysInfo->MaxRootEntries * FILE_ENTRY_SIZE;
  //initialize root and data region
  for (u_int8_t *begin = (u_int8_t*)root, *end = map + volumeSize; begin != end; begin += FILE_ENTRY_SIZE) {
    FILE_t *f = (FILE_t*)begin;
    f->Filename[0] = DIRECTORY_NOT_USED;
  }

  /*
   * Useful calculations
   *
   * // N : cluster number or sector number ( if sysInfo->SectorsPerCluster == 1)
   * FirstSectorOfCluster = DataStartSector + (N - 2) * sysInfo->SectorsPerCluster;
   *
   * FATEntryOfNSector = FAT + (N-2);
   *
   *
   *
   */

  //TODO: parse file name into two parts, and show as xxx.xxx
  FILE_t *working_dir = root;
  /*
  createFile(working_dir, FAT, data, sysInfo, "home", 1);
  working_dir = cd(working_dir, FAT, data, sysInfo, "home");
  createFile(working_dir, FAT, data, sysInfo, "Shihao", 1);
  ls(working_dir, FAT, data, sysInfo);
  pwd(working_dir, data, sysInfo);
  working_dir = cd(working_dir, FAT, data, sysInfo, "Shihao");
  pwd(working_dir, data, sysInfo);*/
	/*
	 * open file, handle errors, create it if necessary.
	 * should end up with map referring to the filesystem.
	 */


	/* You will probably want other variables here for tracking purposes */


	/*
	 * Accept commands, calling accessory functions unless
	 * user enters "quit"
	 * Commands will be well-formatted.
	 */
	char *buffer = NULL;
	size_t size = 0;
	while(getline(&buffer, &size, stdin) != -1)
	{
		/* Basic checks and newline removal */
		size_t length = strlen(buffer);
		if(length == 0)
		{
			continue;
		}
		if(buffer[length-1] == '\n')
		{
			buffer[length-1] = '\0';
		}

		/* TODO: Complete this function */
		/* You do not have to use the functions as commented (and probably can not)
		 *	They are notes for you on what you ultimately need to do.
		 */

		if(!strcmp(buffer, "quit"))
		{
			break;
		}
		else if(!strncmp(buffer, "dump ", 5))
		{
			if(isdigit(buffer[5]))
			{
				//dump(stdout, atoi(buffer + 5));
			}
			else
			{
				char *filename = buffer + 5;
				char *space = strstr(buffer+5, " ");
				*space = '\0';
				//open and validate filename
				//dumpBinary(file, atoi(space + 1));
			}
		}
		else if(!strncmp(buffer, "usage", 5))
		{
			usage(map, root, FAT);
		}
		else if(!strncmp(buffer, "pwd", 3))
		{
          pwd(working_dir, data, sysInfo);
          printf("\n");
		}
		else if(!strncmp(buffer, "cd ", 3))
		{
          FILE_t *dir = cd(working_dir, FAT, data, sysInfo, buffer+3);
          if (dir != NULL)
            working_dir = dir;
          else
            printf("cd: no such file or directory: %s\n", buffer+3);
		}
		else if(!strncmp(buffer, "ls", 2))
		{
          ls(working_dir, FAT, data, sysInfo);
          printf("\n");
		}
		else if(!strncmp(buffer, "mkdir ", 6))
		{
          createFile(working_dir, FAT, data, sysInfo, buffer + 6, 1);
		}
		else if(!strncmp(buffer, "cat ", 4))
		{
			//cat(buffer + 4);
		}
		else if(!strncmp(buffer, "write ", 6))
		{
			char *filename = buffer + 6;
			char *space = strstr(buffer+6, " ");
			*space = '\0';
			size_t amt = atoi(space + 1);
			space = strstr(space+1, " ");

			char *data = generateData(space+1, amt<<1);
			//write(filename, amt, data);
			free(data);
		}
		else if(!strncmp(buffer, "append ", 7))
		{
			char *filename = buffer + 7;
			char *space = strstr(buffer+7, " ");
			*space = '\0';
			size_t amt = atoi(space + 1);
			space = strstr(space+1, " ");

			char *data = generateData(space+1, amt<<1);
			//append(filename, amt, data);
			free(data);
		}
		else if(!strncmp(buffer, "getpages ", 9))
		{
			//getpages(buffer + 9);
		}
		else if(!strncmp(buffer, "get ", 4))
		{
			char *filename = buffer + 4;
			char *space = strstr(buffer+4, " ");
			*space = '\0';
			size_t start = atoi(space + 1);
			space = strstr(space+1, " ");
			size_t end = atoi(space + 1);
			//get(filename, start, end);
		}
		else if(!strncmp(buffer, "rmdir ", 6))
		{
			//rmdir(buffer + 6);
		}
		else if(!strncmp(buffer, "rm -rf ", 7))
		{
			//rmForce(buffer + 7);
		}
		else if(!strncmp(buffer, "rm ", 3))
		{
			//rm(buffer + 3);
		}
		else if(!strncmp(buffer, "scandisk", 8))
		{
			//scandisk();
		}
		else if(!strncmp(buffer, "undelete ", 9))
		{
          undeleteFile(working_dir, FAT, data, sysInfo, buffer+9);
		}



		free(buffer);
		buffer = NULL;
	}
	free(buffer);
	buffer = NULL;

}

/*
 * help() - Print a help message.
 */
void help(char *progname)
{
	printf("Usage: %s [FILE]...\n", progname);
	printf("Loads FILE as a filesystem. Creates FILE if it does not exist\n");
	exit(0);
}

/*
 * main() - The main routine parses arguments and dispatches to the
 * task-specific code.
 */
int main(int argc, char **argv)
{
	/* for getopt */
	long opt;

	/* run a student name check */
	check_student(argv[0]);

	/* parse the command-line options. For this program, we only support */
	/* the parameterless 'h' option, for getting help on program usage. */
	while((opt = getopt(argc, argv, "h")) != -1)
	{
		switch(opt)
		{
		case 'h':
			help(argv[0]);
			break;
		}
	}

	if(argv[1] == NULL)
	{
		fprintf(stderr, "No filename provided, try -h for help.\n");
		return 1;
	}

	filesystem(argv[1]);
	return 0;
}
