#ifndef STRUCTS_H
#define STRUCTS_H

#include <ntsid.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/mman.h>
/*
 *
 * Define page/sector structures here as well as utility structures
 * such as directory entries.
 *
 * Sectors/Pages are 512 bytes
 * The filesystem is 4 megabytes in size.
 * You will have 8K pages total.
 *
 */

#define FREE_CLUSTER      0x0000
#define END_OF_FILE       0xFFFF
#define RESERVED_CLUSTER  0xFF00

#define MAX_LEN_OF_SFN 11
#define MAX_LEN_OF_LFN 255

typedef struct BootSector {
  u_int8_t JumpCode[1]; // Code to jump to bootstrap code;
  u_int8_t OemID; // Oed ID
  u_int16_t BytesPerSector; // Bytes per Sector
  u_int8_t SectorsPerCluster; // Sectors per cluster
  u_int16_t ReservedSectors; // Reserved sectors from the start of the volume
  u_int8_t FATCopies; // Number of FAT copies
  u_int16_t MaxRootEntries; // Number of possible root entries
  u_int16_t TotalSectors; // Number of sectors when volume size is less than 32 Mb
  u_int8_t MediaDescriptor; // Media descriptor
  u_int16_t SectorsPerFAT; // Sectors per FAT
  u_int16_t SectorsPerTrack; // Sectors per track
  u_int16_t Heads; // Number of Heads
  u_int32_t HiddenSectors; // Number of hidden sectors in the partition;
  u_int32_t LargeSectors; // Number of sectors when volume size is greater than 32 Mb
  u_int8_t DriveNumber; // Drive Number
  u_int8_t ExtendedSignature; // indicates that the next three fields are available
  u_int32_t VolumeSerialNumber; // Volume Serial Number
  u_int8_t VolumeLable[11]; // Volume Label - Should be the same as in the root directory
  u_int8_t FileSystemType[8]; // File System Type, should be "FAT16"
  u_int8_t BootstrapCode[448]; // Bootstrap Code
  u_int16_t BootSectorSignature; // Boot Sector Signature
} BootSector;

#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIEVE       0x20
#define ATTR_LONE_FILE_NAME 0x0F

#define DIRECTORY_NOT_USED          0xE5
#define DIRECTORY_NOT_USED_AND_LAST 0x00

typedef struct FileEntry {
  u_int8_t Filename[11]; // 8.3 format, Filename[0] indicates status
  u_int8_t Attr; // Attribute Byte
  u_int8_t ReservedWinNT; // reserved for WindowsNT
  u_int8_t Creation;
  u_int16_t CreationTime;
  u_int16_t CreationDate;
  u_int16_t LastAccessDate;
  u_int16_t FstCLusHI; // Always zero on FAT16
  u_int16_t LastWriteTime;
  u_int16_t LastWriteDate;
  u_int16_t FirstClusterNo;
  u_int32_t FileSize;
} FILE_t; //

#define FILE_ENTRY_SIZE sizeof(FILE_t)

#define Last_LFN 0x40

typedef struct LongFileName
{
  u_int8_t sequenceNo;            // Sequence number, 0xe5 for
  u_int8_t fileName_Part1[10];    // file name part
  u_int8_t fileattribute;         // File attibute
  u_int8_t reserved_1;
  u_int8_t checksum;              // Checksum
  u_int8_t fileName_Part2[12];    // WORD reserved_2;
  u_int16_t FirstClusterNo; // Must be zero
  u_int8_t fileName_Part3[4];
} LFN;

typedef struct SoftLink {
  u_int8_t Filename[11];
  u_int8_t reserved[13];
  FILE_t *fp;
} SoftLink;


void initFileEntry(u_int8_t *working_dir, u_int8_t *fp, char *filename, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, int isDir);
FILE_t* createFile(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *filename, int isDir);
FILE_t* cd(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *filename);

void ls(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo);
void pwd(FILE_t *working_dir, u_int8_t *data, BootSector *sysInfo);

#endif
