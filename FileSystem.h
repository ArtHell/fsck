#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <iostream>
#include <ctype.h>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include "ext2_fs.h"

using namespace std;

#if defined(__FreeBSD__)
#define lseek64 lseek
#endif
extern int64_t lseek64(int, int64_t, int);

const int SECTOR_SIZE_BYTES = 512;
const unsigned int PARTITION_RECORD_SIZE = 16;

struct PartitionEntry {
  unsigned int partitionNumber;
  unsigned int type;
  unsigned int startSector;
  unsigned int length;
  struct PartitionEntry *next;
};

struct InodeData {
  unsigned int inodeNumber;
  unsigned int fileType;
  unsigned int fileLength;
  unsigned int hardLinksQt;
  unsigned int dataBlocksQt;
  unsigned int dataBlocksPointers[15];
};

class FileSystem {
 private:

  /*
   * Data
   */

  int device;
  int extBootRecordOffset;
  unsigned int lostFoundInode;
  unsigned int firstRootBataBlock;
  struct ext2_super_block super_block;
  unsigned char superblockBuffer[6 * 512];
  unsigned int blockSize;
  unsigned int *inodeMap;
  unsigned int *inodeLinkCount;
  unsigned int *blockMap;

  /*
   * Methods
   */

  void readSectors(int64_t, unsigned int, void*);
  void writeSectors(int64_t, unsigned int, void*);
  PartitionEntry* readPartitionEntry(unsigned char*, int, int);
  PartitionEntry* readPartitionTable(int, int, int);
  unsigned int getValueFromBytes(unsigned char*, int, int);
  unsigned int getInodeTableBlockNumber(unsigned int);
  unsigned int getBlockStartingByte(int);
  unsigned int getBlockSector(PartitionEntry*, unsigned int);
  unsigned int getInodeStartingByte(unsigned int);
  void writeInodeEntry(PartitionEntry*, unsigned int);
  unsigned int parseFilesystem(PartitionEntry*, unsigned int, unsigned int,
                               unsigned int, unsigned int, int);
  unsigned int readIndirectDataBlocks(PartitionEntry*, unsigned int,
                                      unsigned int, unsigned int, unsigned int,
                                      int, int, int);
  unsigned int readDataBlocks(PartitionEntry*, unsigned int, unsigned int,
                                unsigned int*, int, int, int);
  void UpdateHardLinkCounter(PartitionEntry*, unsigned int, unsigned int);
  InodeData readInode(PartitionEntry*, unsigned int);
  int checkInodeBitmap(PartitionEntry*, unsigned int);
  int checkBlockBitmap(PartitionEntry*, unsigned int);
  void setBlockBitmap(PartitionEntry*, unsigned int, int);
  int getIndirectDataBlockQt(PartitionEntry*, unsigned int, unsigned int);
  int getDataBlockQt(PartitionEntry*, unsigned int*);

 public:

  FileSystem(const char*);
  virtual ~FileSystem();

  int startFileSystemChecking();

  void readSuperblock(PartitionEntry*);
  void readRootInode(PartitionEntry*);

  PartitionEntry* getPartitionTable(int, int);
  PartitionEntry *getPartitionEntry(PartitionEntry*, unsigned int);

  void setInfo();
  void freeInfo();
};

#endif /* FILESYSTEM_H_ */
