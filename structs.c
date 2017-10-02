#include"structs.h"

/*
 *
 * Definition of structs in structs.h
 *
 */


int isRootDirectory(u_int8_t *working_dir) {
  FILE_t *dir = (FILE_t*)working_dir;
  return dir->Attr == ATTR_VOLUME_ID;
}

/*
 * Initialize entries in cluster to DIRECTORY_NOT_USED state;
 */
void initCluster(u_int16_t clusterNo,
                 u_int8_t *dataRegion,
                 BootSector *sysInfo)
{
  u_int8_t *begin = dataRegion + (clusterNo - 2)
                    * sysInfo->SectorsPerCluster
                    * sysInfo->BytesPerSector;
  u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
  while (begin != end) {
    FILE_t *f = (FILE_t*)begin;
    f->Filename[0] = DIRECTORY_NOT_USED;
    begin += FILEENTRYSIZE;
  }
}

/*
 * Initialize entries in FAT to FREECLUSTER
 */
void initFATRegion(u_int8_t *begin, u_int8_t *end) {
  while (begin != end) {
    u_int16_t *f = (u_int16_t *)begin;
    *f = FREECLUSTER;
    begin += 2; // size of u_int16_t;
  }
}

/*
 * Initialize fields in File_t
 * 1. Find a free cluster
 * 2. Initialize that cluster
 * 3. Bind cluster number to File_t->FirstClusterNo
 */
void initFileEntry(FILE_t *file,
                   u_int16_t *FAT,
                   u_int8_t *dataRegion,
                   BootSector *sysInfo,
                   char *filename)
{
  strcpy(file->Filename, filename);
  file->FileSize = 0;
  u_int16_t N = 2;
  while (FAT[N] != FREECLUSTER)
    ++N;
  initCluster(N, dataRegion, sysInfo);
  FAT[N] = ENDOFFILE;
  file->FirstClusterNo = N;
}

/*
 * Create two directories( '.' and '..') in every subdirectories
 */
void initDirContent(FILE_t *cur_dir,
                    u_int8_t *cur_dir_content,
                    u_int8_t *parent_dir_content)
{
  FILE_t *point = (FILE_t*)cur_dir_content;
  memcpy(point, cur_dir, FILEENTRYSIZE);
  strcpy(point->Filename, ".         "); // must be 11 characters

  FILE_t *point_point = (FILE_t*)(cur_dir_content + FILEENTRYSIZE);
  memcpy(point_point, (FILE_t*)parent_dir_content, FILEENTRYSIZE);
  strcpy(point_point->Filename, "..        "); // must be 11 characters
}


/*
 * create a file or directory in current working directory
 * @param isDir - 0 file, 1 directory
 */
FILE_t* createFile(u_int8_t *working_dir,
                   u_int16_t *FAT,
                   u_int8_t *data,
                   BootSector *sysInfo,
                   char *filename,
                   int isDir)
{
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = working_dir + FILEENTRYSIZE; // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED) {
        initFileEntry(f, FAT, data, sysInfo, filename);
        if (isDir) {
          f->Attr = ATTR_DIRECTORY;
          initDirContent(f, data + (f->FirstClusterNo-2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector, working_dir);
        }
        return f;
      }
      else if (strcmp(f->Filename, filename) == 0){
        printf("File Already Exists");
        return NULL;
      }
      begin += FILEENTRYSIZE;
    }

    return NULL;
  }
  else {
    u_int16_t clusterNo = ((FILE_t *)working_dir)->FirstClusterNo;
    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        if (f->Filename[0] == DIRECTORY_NOT_USED) {
          initFileEntry(f, FAT, data, sysInfo, filename);
          if (isDir) {
            f->Attr = ATTR_DIRECTORY;
            initDirContent(f, data + (f->FirstClusterNo-2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector, working_dir);
          }
          return f;
        }
        else if (strcmp(f->Filename, filename) == 0) {
          printf("File Already Exists");
          return NULL;
        }
        begin += FILEENTRYSIZE;
      }
      clusterNo = FAT[clusterNo]; // search in next cluster
    } while (clusterNo != ENDOFFILE);

    return NULL;
  }
}

void ls(u_int8_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = working_dir + FILEENTRYSIZE; // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        break;
      printf("%s\n", f->Filename);
      begin += FILEENTRYSIZE;
    }
  }
  else {
    u_int16_t clusterNo = ((FILE_t *)working_dir)->FirstClusterNo;
    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          break;
        printf("%s\n", f->Filename);
        begin += FILEENTRYSIZE;
      }
      clusterNo = FAT[clusterNo]; // find in next sector
    } while (clusterNo != ENDOFFILE);
  }
}

u_int8_t* cd(u_int8_t *working_dir,
             u_int16_t *FAT,
             u_int8_t *data,
             BootSector *sysInfo,
             char *dir_name)
{
  if (isRootDirectory(working_dir)) {
    if (strcmp(dir_name, ONE_POINT) == 0 || strcmp(dir_name, TWO_POINT) == 0)
      return working_dir;
    u_int8_t *begin = working_dir + FILEENTRYSIZE; // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        return NULL;
      if (f->Attr == ATTR_DIRECTORY && strcmp(f->Filename, dir_name) == 0)
        return data + (f->FirstClusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      begin += FILEENTRYSIZE;
    }

    return NULL; // dir not found
  }
  else {
    u_int16_t clusterNo = ((FILE_t *)working_dir)->FirstClusterNo;
    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          return NULL;
        if ((f->Attr == ATTR_DIRECTORY || f->Attr == ATTR_VOLUME_ID) && strcmp(f->Filename, dir_name) == 0) {
          if (f->Attr == ATTR_VOLUME_ID) // cd to root dir
            return (u_int8_t*)FAT + sysInfo->FATCopies * sysInfo->SectorsPerFAT * sysInfo->BytesPerSector; // ROOT region
          return data + (f->FirstClusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
        }
        begin += FILEENTRYSIZE;
      }
      clusterNo = FAT[clusterNo]; // find in next sector
    } while (clusterNo != ENDOFFILE);

    return NULL; // dir not found
  }

}