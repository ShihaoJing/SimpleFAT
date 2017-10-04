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
 * Initialize fields in File_t
 * Note: Assume the remaining memory region in current cluster can hold all LFN entries
 * 1. Find a free cluster
 * 2. Initialize that cluster
 * 3. Bind cluster number to File_t->FirstClusterNo
 */
void initFileEntry(u_int8_t *fp,
                   char *filename,
                   u_int16_t *FAT,
                   u_int8_t *dataRegion,
                   BootSector *sysInfo,
                   int isDir)
{
  FILE_t *f = NULL;
  u_int16_t N = 2;
  while (FAT[N] != FREE_CLUSTER)
    ++N;
  FAT[N] = END_OF_FILE;

  //TODO: //bind after cluster initialized
  f->FirstClusterNo = N;

  if (strlen(filename) > MAX_LEN_OF_SFN) {
    //Each LFN can represent up to 13 chars.
    int len = strlen(filename);
    int counts = (len % 13 == 0) ? len / 13 :  len / 13 + 1;

    f = (FILE_t*)fp + counts;
    f->Attr |= ATTR_ARCHIEVE; // indicate this entry is a SFN associated with LFN
    memcpy(f->Filename, filename, sizeof(f->Filename)); // just to indicate a non-empty entry

    LFN *lfn = (LFN*)(f-1);
    for (int i = 0; i < counts; ++i, --lfn) {
      lfn->fileattribute |= ATTR_LONE_FILE_NAME;
      lfn->sequenceNo = i + 1;
      int len_to_copy = strlen(filename + i * 13) >= 13 ? 13 : strlen(filename + i * 13);
      memcpy(lfn->fileName_Part1, filename + i * 13, len_to_copy);
    }
    ++lfn;
    lfn->sequenceNo |= Last_LFN; // mark last LFN

    //TODO:
    /*FILE_t *point = (FILE_t*)cur_dir_content;
    memcpy(point, cur_dir, FILE_ENTRY_SIZE);
    point->Attr |= ATTR_HIDDEN;
    //strcpy(point->Filename, ".         ");

    FILE_t *point_point = (FILE_t*)(cur_dir_content + FILE_ENTRY_SIZE);
    memcpy(point_point, (FILE_t*)parent_dir_content, FILE_ENTRY_SIZE);
    memcpy(point_point->Filename, TWO_POINT_FILE_ENTRY, 11);
    point->Attr |= ATTR_HIDDEN;*/
  }
  else {
    //NOTE: filename should be padding with empty, here copy directly to make search(life) easy
    f = (FILE_t*)fp;
    memcpy(f->Filename, filename, strlen(filename));
  }

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
  if (strlen(filename) > MAX_LEN_OF_LFN || strlen(filename) == 0)
  {
    printf("%s", "Length of filename must be within range from 1 to 255");
    return NULL;
  }
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = working_dir + FILE_ENTRY_SIZE; // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED) {
        initFileEntry(begin, filename, FAT, data, sysInfo, isDir);
        return f;
      }
      else if (strcmp(f->Filename, filename) == 0){
        printf("File Already Exists");
        return NULL;
      }
      begin += FILE_ENTRY_SIZE;
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
        if (f->Attr & ATTR_HIDDEN) { // tricky solution to skip first two entries
          begin += FILE_ENTRY_SIZE;
          continue;
        }
        if (f->Filename[0] == DIRECTORY_NOT_USED) {
          initFileEntry(begin, filename, FAT, data, sysInfo, isDir);
          return f;
        }
        else if (strcmp(f->Filename, filename) == 0) {
          printf("File Already Exists");
          return NULL;
        }
        begin += FILE_ENTRY_SIZE;
      }
      clusterNo = FAT[clusterNo]; // search in next cluster
    } while (clusterNo != END_OF_FILE);

    return NULL;
  }
}

void ls(u_int8_t *working_dir, u_int16_t *FAT, u_int8_t *data, BootSector *sysInfo) {
  if (isRootDirectory(working_dir)) {
    u_int8_t *begin = working_dir + FILE_ENTRY_SIZE; // skip the reserved entry in Root
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
        if (f->Attr & ATTR_HIDDEN) { // skip first two
          printf(".\t..\t");
          begin += 2 * FILE_ENTRY_SIZE;
          continue;
        }
        if (f->Filename[0] == DIRECTORY_NOT_USED)
          break;
        printf("%s\t", f->Filename);
        begin += FILE_ENTRY_SIZE;
      }
      clusterNo = FAT[clusterNo]; // find in next sector
    } while (clusterNo != END_OF_FILE);
  }
}

u_int8_t* cd(u_int8_t *working_dir,
             u_int16_t *FAT,
             u_int8_t *data,
             BootSector *sysInfo,
             char *dir_name)
{
  if (isRootDirectory(working_dir)) {
    if (strcmp(dir_name, ONE_POINT) == 0 || strcmp(dir_name, TWO_POINT_FILE_ENTRY) == 0)
      return working_dir;
    u_int8_t *begin = working_dir + FILE_ENTRY_SIZE; // skip the reserved entry in Root
    u_int8_t *end = data;
    while (begin != end) {
      FILE_t *f = (FILE_t*)begin;
      if (f->Filename[0] == DIRECTORY_NOT_USED)
        return NULL;
      if (f->Attr == ATTR_DIRECTORY && strcmp(f->Filename, dir_name) == 0)
        return data + (f->FirstClusterNo - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
      begin += FILE_ENTRY_SIZE;
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
        begin += FILE_ENTRY_SIZE;
      }
      clusterNo = FAT[clusterNo]; // find in next sector
    } while (clusterNo != END_OF_FILE);

    return NULL; // dir not found
  }

}

void pwd(u_int8_t *working_dir,
         u_int16_t *FAT,
         u_int8_t *data,
         BootSector *sysInfo)
{
  if (isRootDirectory(working_dir)) {
    printf("/");
    return;
  }
  FILE_t *dir = (FILE_t*)working_dir;
  u_int16_t parentCluster = (dir + 1)->FirstClusterNo;
  if (parentCluster == 0) {
    u_int8_t *ROOT = (u_int8_t*)FAT + sysInfo->FATCopies * sysInfo->SectorsPerFAT * sysInfo->BytesPerSector;
    pwd(ROOT, FAT, data, sysInfo);
  }
  else {
    u_int8_t *ROOT = data + (parentCluster - 2) * sysInfo->SectorsPerCluster * sysInfo->BytesPerSector;
    pwd(ROOT, FAT, data, sysInfo);
  }
  printf("%s/", dir->Filename);
}