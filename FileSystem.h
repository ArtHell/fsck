#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#if defined(__FreeBSD__)
#define lseek64 lseek
#endif
extern int64_t lseek64(int, int64_t, int);

const unsigned int SECTOR_SIZE_BYTES = 512;
const unsigned int PARTITION_RECORD_SIZE = 16;

typedef struct partition_entry {
  unsigned int partitionNumber;
  unsigned int type;
  unsigned int startSector;
  unsigned int length;
  struct partition_entry *next;
} partition_entry;

class FileSystem {
 private:
  int device;
  int extBootRecordOffset;
  void readSectors(int64_t, unsigned int, void*);
  partition_entry* readPartitionEntry(unsigned char*, int, int);
  partition_entry* readPartitionTable(int, int, int);
 public:
  FileSystem(char*);
  partition_entry* getPartitionTable(int, int);
  virtual ~FileSystem();
};

#endif /* FILESYSTEM_H_ */
