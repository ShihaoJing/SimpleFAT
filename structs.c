#include"structs.h"

/*
 *
 * Definition of structs in structs.h
 *
 */


int isRootDirectory(FILE_t *working_dir) {
  return working_dir->Attr == ATTR_VOLUME_ID;
}



/*
 * Initialize fields in File_t
 * Note: Assume the remaining memory region in current cluster can hold all LFN entries
 * 1. Find a free cluster
 * 2. Initialize that cluster
 * 3. Bind cluster number to File_t->FirstClusterNo
 */
void initFileEntry(u_int8_t *working_dir,
                   u_int8_t *fp,
                   char *filename,
                   u_int16_t *FAT,
                   u_int8_t *dataRegion,
                   BootSector *sysInfo,
                   int isDir)
{
  FILE_t *f = NULL;

  if (strlen(filename) > MAX_LEN_OF_SFN) {
    //Each LFN can represent up to 13 chars.
    int len = strlen(filename);
    int counts = (len % 10 == 0) ? len / 10 :  len / 10 + 1;

    f = (FILE_t*)fp + counts;
    f->Attr |= ATTR_ARCHIEVE; // indicate this entry is a SFN associated with LFN
    memcpy(f->Filename, filename, sizeof(f->Filename)); // just to indicate a non-empty entry

    LFN *lfn = (LFN*)(f-1);
    for (int i = 0; i < counts; ++i, --lfn) {
      lfn->fileattribute |= ATTR_LONE_FILE_NAME;
      lfn->sequenceNo = i + 1;
      int len_to_copy = strlen(filename + i * 10) >= 10 ? 10 : strlen(filename + i * 10);
      memcpy(lfn->fileName_Part1, filename + i * 10, len_to_copy);
    }
    ++lfn;
    lfn->sequenceNo |= Last_LFN; // mark last LFN
  }
  else {
    //NOTE: filename should be padding with empty, here copy directly to make search(life) easy
    f = (FILE_t*)fp;
    memcpy(f->Filename, filename, strlen(filename));
  }

  u_int16_t N = 2;
  while (FAT[N] != FREE_CLUSTER)
    ++N;
  FAT[N] = END_OF_FILE;

  f->FirstClusterNo = N;

  if (isDir) {
    f->Attr |= ATTR_DIRECTORY;
    u_int8_t *dir = dataRegion + (N - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;

    SoftLink *point = (SoftLink*)dir;
    strcpy(point->Filename, ".");
    point->fp = f;

    SoftLink *point_point = (SoftLink*)(dir + FILE_ENTRY_SIZE);
    strcpy(point_point, "..");
    point_point->fp = (FILE_t*)working_dir;
  }
}


/*
 * create a file or directory in current working directory
 * @param isDir - 0 file, 1 directory
 */
FILE_t* createFile(FILE_t *working_dir,
                   u_int16_t *FAT,
                   u_int8_t *data,
                   BootSector *sysInfo,
                   char *filename,
                   int isDir)
{
  if (strlen(filename) > MAX_LEN_OF_LFN || strlen(filename) == 0)
  {
    printf("%s", "Length of filename must be within range from 1 to 255");
    return NULL;
  }
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = (u_int8_t*)(working_dir + 1); // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED) {
        initFileEntry(working_dir, begin, filename, FAT, data, sysInfo, isDir);
        return f;
      }
      else if (strcmp(f->Filename, filename) == 0){
        printf("File %s Already Exists\n", filename);
        return NULL;
      }
      begin += FILE_ENTRY_SIZE;
    }

    return NULL;
  }
  else {
    u_int16_t clusterNo = working_dir->FirstClusterNo;
    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        if (f->Filename[0] == DIRECTORY_NOT_USED) {
          initFileEntry(working_dir, begin, filename, FAT, data, sysInfo, isDir);
          return f;
        }
        else if (strcmp(f->Filename, filename) == 0) {
          printf("File %s Already Exists\n", filename);
          return NULL;
        }
        begin += FILE_ENTRY_SIZE;
      }
      clusterNo = FAT[clusterNo]; // search in next cluster
    } while (clusterNo != END_OF_FILE);

    return NULL;
  }
}

void ls(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {
  if (isRootDirectory(working_dir)) {
    printf(".\t..\t");
    u_int8_t *begin = (u_int8_t*)(working_dir + 1); // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t *) begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        break;
      printf("%s\t", f->Filename);
      begin += FILE_ENTRY_SIZE;
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
        printf("%s\t", f->Filename);
        begin += FILE_ENTRY_SIZE;
      }
      clusterNo = FAT[clusterNo]; // find in next sector
    } while (clusterNo != END_OF_FILE);
  }
}

FILE_t* cd(FILE_t *working_dir,
             u_int16_t *FAT,
             u_int8_t *data,
             BootSector *sysInfo,
             char *dir_name)
{
  if (isRootDirectory(working_dir)) {
    if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)
      return working_dir;
    u_int8_t *begin = (u_int8_t*)(working_dir + 1); // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        return NULL;
      if (f->Attr == ATTR_DIRECTORY && strcmp(f->Filename, dir_name) == 0)
        return f;
      begin += FILE_ENTRY_SIZE;
    }

    return NULL; // dir not found
  }
  else {
    u_int16_t clusterNo = working_dir->FirstClusterNo;
    if (strcmp(dir_name, ".") == 0)
      return working_dir;
    if (strcmp(dir_name, "..") == 0) {
      FILE_t *dir_content = (FILE_t*)(data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector);
      ++dir_content;
      return ((SoftLink*)dir_content)->fp;
    }

    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          return NULL;
        if (strcmp(f->Filename, dir_name) == 0) {
          return f;
        }
        begin += FILE_ENTRY_SIZE;
      }
      clusterNo = FAT[clusterNo]; // find in next sector
    } while (clusterNo != END_OF_FILE);

    return NULL; // dir not found
  }
}

void pwd(FILE_t *working_dir, u_int8_t *data, BootSector *sysInfo)
{
  if (isRootDirectory(working_dir)) {
    printf("/");
    return;
  }
  FILE_t *dir_content = (FILE_t*)(data + (working_dir->FirstClusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector);
  ++dir_content;
  pwd(((SoftLink*)dir_content)->fp, data, sysInfo);
  printf("%s/", working_dir->Filename);
}

void undeleteFATEntry(int clusterNo, u_int16_t *FAT) {
  FAT[clusterNo] ^= DELETED_CLUSTER;
  if (FAT[clusterNo] == END_OF_FILE) {
    return;
  }
  undeleteFATEntry(FAT[clusterNo], FAT);
}

void undeleteFile(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *filename) {
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = (u_int8_t*)(working_dir + 1); // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (strcmp(f->Filename, filename) == 0) {
        f->Attr ^= ATTR_DELETED;
        undeleteFATEntry(f->FirstClusterNo, FAT);
        return;
      }
      begin += FILE_ENTRY_SIZE;
    }
  }
  else {
    u_int16_t clusterNo = working_dir->FirstClusterNo;
    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        if (strcmp(f->Filename, filename) == 0) {
          f->Attr ^= ATTR_DELETED;
          undeleteFATEntry(f->FirstClusterNo, FAT);
          return;
        }
        begin += FILE_ENTRY_SIZE;
      }
      clusterNo = FAT[clusterNo]; // find in next sector
    } while (clusterNo != END_OF_FILE);
  }
}