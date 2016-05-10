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
#include "ext2_fs.h" // structures of ext2 system

#if defined(__FreeBSD__)
#define lseek64 lseek
#endif
extern int64_t lseek64(int, int64_t, int);

const unsigned int SECTOR_SIZE_BYTES = 512;
const unsigned int PARTITION_RECORD_SIZE = 16;
const unsigned int BLOCK_SIZE= 1024;
static unsigned char superblockBuffer[6 * 512];
static struct ext2_super_block super_block;

typedef struct partition_entry {
  unsigned int partitionNumber;
  unsigned int type;
  unsigned int startSector;
  unsigned int length;
  struct partition_entry *next;
} partition_entry;

typedef struct inode_data {
  unsigned int inode_no;
  unsigned int file_type;
  unsigned int file_length;
  unsigned int no_data_blocks;
  unsigned int pointers_data_block[15];
} inode_data;

class FileSystem {
 private:

  int device;
  int extBootRecordOffset;

  void readSectors(int64_t, unsigned int, void*);
  partition_entry* readPartitionEntry(unsigned char*, int, int);
  partition_entry* readPartitionTable(int, int, int);
  unsigned int getValueFromBytes(unsigned char*, int, int);
  unsigned int getInodeTableBlockNumber(unsigned int);
  unsigned int getBlockStartingByte(int);
  unsigned int getBlockSector(partition_entry*, unsigned int);
  unsigned int getInodeStartingByte(unsigned int);
  void scanDirectoryBlock(partition_entry*, unsigned int);
  void parseFilesystem(partition_entry*, unsigned int);
  void readIndirectDataBlocks(partition_entry*, unsigned int, unsigned int);
  int checkInodeBitmap(partition_entry*, unsigned int);
  int checkBlockBitmap(partition_entry*, unsigned int);
  int getIndirectDataBlockQt(partition_entry*, unsigned int,
                                    unsigned int);
  int getDataBlockQt(partition_entry*, unsigned int*);
  inode_data readInode(partition_entry*, unsigned int);
  void readDataBlocks(partition_entry*, unsigned int*);

 public:

  FileSystem(const char*);
  partition_entry* getPartitionTable(int, int);
  partition_entry *getPartitionEntry(partition_entry*, unsigned int);
  void readSuperblock(partition_entry*);
  void readRootInode(partition_entry*);
  virtual ~FileSystem();
};

#endif /* FILESYSTEM_H_ */
