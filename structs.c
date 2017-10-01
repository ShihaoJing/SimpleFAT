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

void initSector(u_int8_t *begin, u_int8_t *end) {
  while (begin != end) {
    FILE_t *f = (FILE_t*)begin;
    f->Filename[0] = DIRECTORY_NOT_USED;
    begin += sizeof(FILE_t);
  }
}

void initFATRegion(u_int8_t *begin, u_int8_t *end) {
  while (begin != end) {
    u_int16_t *f = (u_int16_t *)begin;
    *f = FREECLUSTER;
    begin += sizeof(u_int16_t);
  }
}

void initFile(FILE_t *file, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *filename) {
  strcpy(file->Filename, filename);
  file->FileSize = 0;
  u_int16_t N = 2;
  while (FAT[N] != FREECLUSTER)
    ++N;
  FAT[N] = ENDOFFILE;
  file->SectorNumber = N;
  u_int8_t *sectorBegin = data + (N-2)*sysInfo->BytesPerSector;
  initSector(sectorBegin, sectorBegin + sysInfo->BytesPerSector);
}

void initDir(FILE_t *cur_dir, u_int8_t *cur_dir_content, u_int8_t *parent_dir_content)
{
  FILE_t *self = (FILE_t*)cur_dir_content;
  memcpy(self, cur_dir, sizeof(FILE_t));
  strcpy(self->Filename, ".         ");

  FILE_t *p = (FILE_t*)parent_dir_content;
  FILE_t *parent = (FILE_t*)(cur_dir_content + sizeof(FILE_t));
  memcpy(parent, (FILE_t*)parent_dir_content, sizeof(FILE_t));
  strcpy(parent->Filename, "..        ");
  if (isRootDirectory(parent_dir_content))
    parent->SectorNumber = 0;
}


FILE_t* createFile(u_int8_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *filename, int isDir) {
  if (isRootDirectory(working_dir)) {
    u_int8_t *p = working_dir + sizeof(FILE_t); // skip the reserved entry in Root
    while (p != data) {
      FILE_t *f = (FILE_t*)p;
      if (f->Filename[0] == DIRECTORY_NOT_USED) {
        initFile(f, FAT, data, sysInfo, filename);
        if (isDir) {
          f->Attr = ATTR_DIRECTORY;
          initDir(f, data+(f->SectorNumber-2) * sysInfo->BytesPerSector, working_dir);
        }
        return f;
      }
      else if (strcmp(f->Filename, filename) == 0){
        printf("File Already Exists");
        return NULL;
      }
      p += sizeof(FILE_t);
    }
    return NULL;
  }
  else {
    u_int16_t sectorNum = ((FILE_t *)working_dir)->SectorNumber;
    do {
      u_int8_t *sectorBegin = data + (sectorNum - 2) * sysInfo->BytesPerSector;
      u_int8_t *sectorEnd = sectorBegin + sysInfo->BytesPerSector;
      while (sectorBegin != sectorEnd) {
        FILE_t *f = (FILE_t *) sectorBegin;
        if (f->Filename[0] == DIRECTORY_NOT_USED) {
          initFile(f, FAT, data, sysInfo, filename);
          if (isDir) {
            f->Attr = ATTR_DIRECTORY;
            u_int8_t *newSectorBegin = data+ (f->SectorNumber - 2) * sysInfo->BytesPerSector;
            initDir(f, newSectorBegin, working_dir);
        }
          return f;
        }
        else if (strcmp(f->Filename, filename) == 0) {
          printf("File Already Exists");
          return NULL;
        }
        sectorBegin += sizeof(FILE_t);
      }
      sectorNum = FAT[sectorNum]; // find in next sector
    } while (sectorNum != ENDOFFILE);
    return NULL;
  }
}

void ls(u_int8_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {
  if (isRootDirectory(working_dir)) {
    u_int8_t *p = working_dir + sizeof(FILE_t); // skip the reserved entry in Root
    while (p != data) {
      FILE_t *f = (FILE_t*)p;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        break;
      printf("%s\n", f->Filename);
      p += sizeof(FILE_t);
    }
  }
  else {
    u_int16_t sectorNum = ((FILE_t *)working_dir)->SectorNumber;
    do {
      u_int8_t *sectorBegin = data + (sectorNum - 2) * sysInfo->BytesPerSector;
      u_int8_t *sectorEnd = sectorBegin + sysInfo->BytesPerSector;
      while (sectorBegin != sectorEnd) {
        FILE_t *f = (FILE_t *) sectorBegin;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          break;
        printf("%s\n", f->Filename);
        sectorBegin += sizeof(FILE_t);
      }
      sectorNum = FAT[sectorNum]; // find in next sector
    } while (sectorNum != ENDOFFILE);
  }
}

u_int8_t* cd(u_int8_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *dir_name) {
  if (isRootDirectory(working_dir)) {
    if (strcmp(dir_name, ONE_POINT) == 0 || strcmp(dir_name, TWO_POINT) == 0)
      return working_dir;
    u_int8_t *sectorBegin = working_dir + sizeof(FILE_t); // skip the reserved entry in Root
    u_int8_t *sectorEnd = data;
    while (sectorBegin != sectorEnd) {
      FILE_t *f = (FILE_t*)sectorBegin;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        return NULL;
      if (f->Attr == ATTR_DIRECTORY && strcmp(f->Filename, dir_name) == 0)
        return data + (f->SectorNumber - 2) * sysInfo->BytesPerSector;
      sectorBegin += sizeof(FILE_t);
    }
  }
  else {
    u_int16_t sectorNum = ((FILE_t *)working_dir)->SectorNumber;
    do {
      u_int8_t *sectorBegin = data + (sectorNum - 2) * sysInfo->BytesPerSector;
      u_int8_t *sectorEnd = sectorBegin + sysInfo->BytesPerSector;
      while (sectorBegin != sectorEnd) {
        FILE_t *f = (FILE_t *) sectorBegin;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          return NULL;
        if ((f->Attr == ATTR_DIRECTORY || f->Attr == ATTR_VOLUME_ID) && strcmp(f->Filename, dir_name) == 0) {
          if (f->Attr == ATTR_VOLUME_ID) // cd to root dir
            return (u_int8_t*)FAT + sysInfo->FATCopies * sysInfo->SectorsPerFAT * sysInfo->BytesPerSector; // start pos of root region
          return data + (f->SectorNumber - 2) * sysInfo->BytesPerSector;
        }
        sectorBegin += sizeof(FILE_t);
      }
      sectorNum = FAT[sectorNum]; // find in next sector
    } while (sectorNum != ENDOFFILE);
  }
  return NULL; // dir not found
}