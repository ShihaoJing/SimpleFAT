#include"structs.h"

/*
 *
 * Definition of structs in structs.h
 *
 */


int isRootDirectory(FILE_t *working_dir) {
  return working_dir->Attr == ATTR_VOLUME_ID;
}

int isEmpty(FILE_t *f, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {
  u_int16_t clustNo = f->FirstClusterNo;
  do {
    u_int8_t *begin = data + (clustNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector + 2 * FILE_ENTRY_SIZE;
    u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
    while (begin != end) {
      FILE_t *f = (FILE_t *) begin;
      if (f->Filename[0] != DIRECTORY_NOT_USED) {
        return 0;
      }
      begin += FILE_ENTRY_SIZE;
    }
    clustNo = FAT[clustNo]; // search in next cluster
  } while (clustNo != END_OF_FILE);
  return 1;
}

/*
 * return file entry with filename
 * if file not found, return first empty entry
 */
FILE_t* searchFile(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *filename) {
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = (u_int8_t*)(working_dir + 1); // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      begin += FILE_ENTRY_SIZE;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        return f;
      if (f->Attr & ATTR_DELETED)
        return f;
      if (strcmp(f->Filename, filename) == 0 && !(f->Attr & ATTR_DELETED)){
        return f;
      }
    }
    return NULL;
  }
  else {
    u_int16_t clusterNo = working_dir->FirstClusterNo;
    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector
                        + RESERVED_DIRECTORY_REGION_SIZE;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        begin += FILE_ENTRY_SIZE;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          return f;
        if (f->Attr & ATTR_DELETED)
          return f;
        if (strcmp(f->Filename, filename) == 0 && !(f->Attr & ATTR_DELETED))
          return f;
      }
      clusterNo = FAT[clusterNo]; // search in next cluster
    } while (clusterNo != END_OF_FILE);
    return NULL;
  }
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
      else if (strcmp(f->Filename, filename) == 0 && !(f->Attr & ATTR_DELETED)){
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
  printf(".\t..\t");
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = (u_int8_t*)(working_dir + 1); // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t *) begin;
      begin += FILE_ENTRY_SIZE;
      if (f->Attr & ATTR_DELETED)
        continue;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        break;
      printf("%s\t", f->Filename);
    }
  }
  else {
    u_int16_t clusterNo = working_dir->FirstClusterNo;
    do {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector
                        + RESERVED_DIRECTORY_REGION_SIZE;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        begin += FILE_ENTRY_SIZE;
        if (f->Attr & ATTR_DELETED)
          continue;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          break;
        printf("%s\t", f->Filename);
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
      if (f->Attr == ATTR_DELETED)
        continue;
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
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector
                        + RESERVED_DIRECTORY_REGION_SIZE;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        if (f->Attr == ATTR_DELETED)
          continue;
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


void rm_rf(FILE_t *file, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {
  file->Attr ^= ATTR_DELETED;
  u_int16_t clusterNo = file->FirstClusterNo;
  do {
    if (file->Attr & ATTR_DIRECTORY) {
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector
                        + RESERVED_DIRECTORY_REGION_SIZE;
      u_int8_t *end = data;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        begin += FILE_ENTRY_SIZE;
        if (f->Attr & ATTR_HIDDEN || f->Attr & ATTR_DELETED)
          continue;
        if (!(f->Attr & ATTR_DIRECTORY)) { // remove file
          u_int16_t cluster = f->FirstClusterNo;
          do {
            uint16_t next = FAT[cluster];
            FAT[cluster] ^= DELETED_CLUSTER;
            cluster = next;
          } while (cluster != END_OF_FILE);
        }
        else { // remove directory
          if (isEmpty(f, FAT, data, sysInfo)) {
            u_int16_t cluster = f->FirstClusterNo;
            do {
              uint16_t next = FAT[cluster];
              FAT[cluster] ^= DELETED_CLUSTER;
              cluster = next;
            } while (cluster != END_OF_FILE);
          } else {
            rm_rf(f, FAT, data, sysInfo);
            f->Attr ^= ATTR_DELETED;
          }
        }
      }
    }
    u_int16_t next = FAT[clusterNo];
    FAT[clusterNo] ^= DELETED_CLUSTER;
    clusterNo = next;
  } while (clusterNo != END_OF_FILE);
}

void rm_dir(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *dir_name) {
  FILE_t *dir = searchFile(working_dir, FAT, data, sysInfo, dir_name);
  if (dir->Filename[0] == DIRECTORY_NOT_USED || dir->Attr & ATTR_DELETED) {
    printf("rmdir: %s does not exist.\n", dir_name);
    return;
  }
  if (!(dir->Attr & ATTR_DIRECTORY)) {
    printf("rmdir: %s is not a directory.\n", dir_name);
    return;
  }
  if (!isEmpty(dir, FAT, data, sysInfo)) {
    printf("rmdir: %s is not empty.\n", dir_name);
    return;
  }
  dir->Attr ^= ATTR_DELETED;
  u_int16_t cluster = dir->FirstClusterNo;
  do {
    uint16_t next = FAT[cluster];
    FAT[cluster] ^= DELETED_CLUSTER;
    cluster = next;
  } while(cluster != END_OF_FILE);
}

void undeleteFile(FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo, char *filename) {
  FILE_t *f = searchFile(working_dir, FAT, data, sysInfo, filename);
  if (f->Filename[0] == DIRECTORY_NOT_USED) {
    printf("undelete: %s does not exist.\n", filename);
    return;
  }
  if (!(f->Attr & ATTR_DELETED)) {
    printf("undelete: %s have not been deleted yet.\n", filename);
    return;
  }
  f->Attr ^= ATTR_DELETED;
  u_int16_t clusterNo = f->FirstClusterNo;
  do {
    FAT[clusterNo] ^= DELETED_CLUSTER;
    clusterNo = FAT[clusterNo];
  } while (clusterNo != END_OF_FILE);
}

//cat: Outputs a file to the console. If used on a directory, say so and reject. If the file does not exist, say so and reject.
void cat(char* filename, FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data,BootSector *sysInfo){
  FILE_t *f = searchFile(working_dir, FAT, data, sysInfo, filename);
  if (f->Filename[0] == DIRECTORY_NOT_USED || f->Attr & ATTR_DELETED) {
    printf("cat: %s does not exist.\n", filename);
    return;
  }
  if (f->Attr & ATTR_DIRECTORY) {
    printf("cat: %s is not a file.\n", filename);
    return;
  }
  u_int8_t *begin = data + (f->FirstClusterNo -2) * sysInfo->SectorsPerCluster * sysInfo-> BytesPerSector;
  printf("%s\n", begin);
}

//Write <amt> bytes of <data> into the specified <file> in the current directory. This overwrites the file if it already exists.
//This creates the file if it did not exist. The data is given as a stream of hex digits.
void writeFile(char* filename, size_t amt, char *input, FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo){
  int found = 0; //false
  FILE_t *f = searchFile(working_dir, FAT, data, sysInfo, filename);
  if (f->Attr & ATTR_DIRECTORY) {
    printf("cat: %s is not a file.\n", filename);
    return;
  }
  if (f->Attr & ATTR_DELETED)
    f->Attr ^= ATTR_DELETED;
  if (f->Filename[0] == DIRECTORY_NOT_USED) {
    printf("writeFile: create a new file\n");
    initFileEntry((u_int8_t*)working_dir, (u_int8_t*)f, filename, FAT, data, sysInfo, 0);
  }
  u_int8_t * dest = data + (f->FirstClusterNo -2) * sysInfo->SectorsPerCluster * sysInfo-> BytesPerSector;
  memcpy(dest, input, strlen(input)+1);
  f->FileSize = strlen(input);
}

//Append <amt> bytes of <data> onto the specified <file> in the current directory.
//This fails, without terminating, if the file does not already exist. The data is given as a stream of hex digits.
void append(char*filename, size_t amt, char *input, FILE_t *working_dir, u_int16_t *FAT, u_int8_t * data, BootSector *sysInfo)
{
  FILE_t *f = searchFile(working_dir, FAT, data, sysInfo, filename);
  if (f->Attr & ATTR_DIRECTORY) {
    printf("append: %s is not a file.\n", filename);
    return;
  }
  if (f->Filename[0] == DIRECTORY_NOT_USED || f->Attr & ATTR_DELETED) {
    printf("append: %s does not exist.\n", filename);
    return;
  }
  u_int8_t *appendStart = data + (f->FirstClusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector
                          + f->FileSize;
  memcpy(appendStart, input, strlen(input)+1);
  f-> FileSize += strlen(input);
}

// "get <file> <start> <end>": Print to the console the bytes from the file in the range [start,end).
// This fails, without terminating, if the file does not already exist.
// Print whatever part of the range is possible.
void get(char* filename, size_t startByte, size_t endByte, FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {
  FILE_t *f = searchFile(working_dir, FAT, data, sysInfo, filename);
  if (f->Attr & ATTR_DIRECTORY) {
    printf("append: %s is not a file.\n", filename);
    return;
  }
  if (f->Filename[0] == DIRECTORY_NOT_USED || f->Attr & ATTR_DELETED) {
    printf("append: %s does not exist.\n", filename);
    return;
  }
  int clusterNo = f->FirstClusterNo;
  u_int8_t *dataRegion = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
  u_int8_t *b = dataRegion + startByte, *e = dataRegion + endByte;
  while (b != e) {
    printf("%c", b++);
  }
}

void getPages(FILE_t *file, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo)
{
  if (file->Attr & ATTR_DIRECTORY) {
    u_int16_t clusterNo = file->FirstClusterNo;
    do {
      printf("%u \n", clusterNo);
      u_int8_t *begin = data + (clusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector
                        + RESERVED_DIRECTORY_REGION_SIZE;
      u_int8_t *end = begin + sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      while (begin != end) {
        FILE_t *f = (FILE_t *) begin;
        begin += FILE_ENTRY_SIZE;
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          return;
        if(f->Attr & ATTR_DIRECTORY)
          getPages(f, FAT, data, sysInfo);
        else {
          u_int16_t cluster = file->FirstClusterNo;
          do {
            printf("%u \n", cluster);
            cluster = FAT[cluster];
          } while (cluster != END_OF_FILE);
        }
      }
      clusterNo = FAT[clusterNo]; // search in next cluster
    } while (clusterNo != END_OF_FILE);
  }
  else {
    u_int16_t clusterNo = file->FirstClusterNo;
    do {
      printf("%u \n", clusterNo);
      clusterNo = FAT[clusterNo];
    } while (clusterNo != END_OF_FILE);
  }
}

//Remove the bytes in the range [start,end) from the specified <file> in the current directory.
// This fails, without terminating, if the file does not already exist
void removeRange(char* filename, int start, int end, FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo)
{
  FILE_t *f = searchFile(working_dir, FAT, data, sysInfo, filename);
  if (f->Attr & ATTR_DIRECTORY) {
    printf("append: %s is not a file.\n", filename);
    return;
  }
  if (f->Filename[0] == DIRECTORY_NOT_USED || f->Attr & ATTR_DELETED) {
    printf("append: %s does not exist.\n", filename);
    return;
  }
  u_int8_t *appendStart = data + (f->FirstClusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector
      + f->FileSize;
  //TODO: remove bytes in range [start, end)
}

//Removes a file and recovers the pages. Report, but do not terminate, if the file is a directory.
void rm(char* filename, FILE_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo){
  FILE_t *f = searchFile(working_dir, FAT, data, sysInfo, filename);
  if (f->Attr & ATTR_DIRECTORY) {
    printf("append: %s is not a file.\n", filename);
    return;
  }
  if (f->Filename[0] == DIRECTORY_NOT_USED || f->Attr & ATTR_DELETED) {
    printf("append: %s does not exist.\n", filename);
    return;
  }
  f->Attr ^= ATTR_DELETED;
  u_int16_t cluster = f->FirstClusterNo;
  do {
    uint16_t next = FAT[cluster];
    FAT[cluster] ^= DELETED_CLUSTER;
    cluster = next;
  } while (cluster != END_OF_FILE);
}


// "scandisk": Validates the system integrity.
// If a sector is marked as allocated but no existing file can access it, it should not be marked as allocated.
// If a file refers to a sector that is not allocated, we potentially have a problem.
// Ask the user if they want to (t)runcate the file (cut it off at that point)
// or (a)llocate the blocks as if they actually belong to the file.
// If a sector is owned, incorrectly, by more than one file, report this but fixing it is out-of-scope.
void scandisk(FILE_t *root_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {

}

void dump(u_int16_t pageNumber, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {

}

void dumpBinary(u_int16_t pageNumber, char *filename, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {

}