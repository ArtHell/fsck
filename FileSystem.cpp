#include "FileSystem.h"

FileSystem::FileSystem(const char* diskImage) {
  if ((device = open(diskImage, O_RDWR)) == -1) {
    perror("Could not open disk");
    exit(-1);
  }
  extBootRecordOffset = 0;
}

void FileSystem::readSectors(int64_t startSector, unsigned int sectorsQt,
                             void *into) {
  ssize_t ret;
  int64_t lret;
  int64_t sectorOffset;
  ssize_t bytesToRead;

  sectorOffset = startSector * SECTOR_SIZE_BYTES;

  if ((lret = lseek64(device, sectorOffset, SEEK_SET)) != sectorOffset) {
    exit(-1);
  }

  bytesToRead = SECTOR_SIZE_BYTES * sectorsQt;

  if ((ret = read(device, into, bytesToRead)) != bytesToRead) {
    exit(-1);
  }
}

partition_entry* FileSystem::readPartitionEntry(unsigned char *partitionBuffer,
                                                int partitionNumber,
                                                int sectorOffset) {

  int type = (int) partitionBuffer[4] & 0xFF;
  if (type != 0x82 && type != 0x00 && type != 0x83 && type != 0x05) {
    return NULL;
  }

  partition_entry *entry = (partition_entry*) malloc(sizeof(partition_entry));
  entry->partitionNumber = partitionNumber;
  entry->type = type;

  if (type == 0x05)
    entry->startSector = ((((int) partitionBuffer[11] & 0xFF) << 24)
        | (((int) partitionBuffer[10] & 0xFF) << 16)
        | (((int) partitionBuffer[9] & 0xFF) << 8)
        | (((int) partitionBuffer[8]) & 0xFF)) + extBootRecordOffset;
  else
    entry->startSector = ((((int) partitionBuffer[11] & 0xFF) << 24)
        | (((int) partitionBuffer[10] & 0xFF) << 16)
        | (((int) partitionBuffer[9] & 0xFF) << 8)
        | (((int) partitionBuffer[8]) & 0xFF)) + sectorOffset;

  entry->length = (((int) partitionBuffer[15] & 0xFF) << 24)
      | (((int) partitionBuffer[14] & 0xFF) << 16)
      | (((int) partitionBuffer[13] & 0xFF) << 8)
      | ((int) partitionBuffer[12] & 0xFF);

  entry->next = NULL;

  return entry;
}

partition_entry* FileSystem::readPartitionTable(int sector, int partitionNumber,
                                                int sectorOffset) {
  unsigned int partitionAddress, i;
  unsigned char partitionBuffer[PARTITION_RECORD_SIZE];
  unsigned char buf[SECTOR_SIZE_BYTES];

  readSectors(sector, 1, buf);

  partitionAddress = 446 + ((partitionNumber - 1) * 16);
  for (i = partitionAddress; i < partitionAddress + PARTITION_RECORD_SIZE;
      i++) {
    partitionBuffer[i - partitionAddress] = buf[i];
  }

  partition_entry *part = readPartitionEntry(partitionBuffer, partitionNumber,
                                             sectorOffset);

  return part;
}

partition_entry* FileSystem::getPartitionTable(int sector, int sectorOffset) {

  int i;
  partition_entry *entry = NULL;
  partition_entry *first = NULL;

  int partitionCount = 4;
  if (sectorOffset != 0)
    partitionCount = 2;

  for (i = 1; i <= partitionCount; i++) {
    partition_entry *temp = readPartitionTable(sector, i, sectorOffset);
    if (entry == NULL) {
      entry = temp;
      first = entry;
    } else if (temp != NULL) {
      entry->next = temp;
      entry = entry->next;
    }
  }

  partition_entry *temp = first;
  partition_entry *end = entry;
  while (temp != end->next) {
    if (temp->type == 5) {
      if (extBootRecordOffset == 0)
        extBootRecordOffset = temp->startSector;
      entry->next = getPartitionTable(temp->startSector, temp->startSector);
      partition_entry *current = entry->next;
      partition_entry *prev = entry;
      while (current != NULL) {
        if (current->type == 5 || current->type == 0) {
          if (current->next == NULL) {
            free(current);
            prev->next = NULL;
          } else {
            prev->next = current->next;
            free(current);
            current = prev->next;
            continue;
          }
        }
        prev = current;
        current = current->next;
      }
    }
    while (entry->next != NULL)
      entry = entry->next;
    temp = temp->next;
  }

  return first;
}

partition_entry* FileSystem::getPartitionEntry(partition_entry *head,
                                                 unsigned int partitionNumber) {
  unsigned int count = 1;
  while (head != NULL) {
    if (partitionNumber == count) {
      return head;
    }
    head = head->next;
    count++;
  }
  return NULL;
}

unsigned int FileSystem::getValueFromBytes(unsigned char *buf, int index,
                                           int size) {
  if (size == 4)
    return ((((int) buf[index + 3] & 0xFF) << 24)
        | (((int) buf[index + 2] & 0xFF) << 16)
        | (((int) buf[index + 1] & 0xFF) << 8) | (((int) buf[index]) & 0xFF));
  else if (size == 2)
    return ((((int) buf[index + 1] & 0xFF) << 8) | ((int) buf[index] & 0xFF));
  else
    exit(-1);
}

unsigned int FileSystem::getInodeTableBlockNumber(unsigned int inode_no) {
  unsigned int groupIndex = (inode_no - 1) / super_block.s_inodes_per_group;
  unsigned int group_offset = (inode_no - 1) % super_block.s_inodes_per_group;
  printf("inode table block number for inode: %d: %d\n", inode_no,
         getValueFromBytes(superblockBuffer, 2048 + (groupIndex * 32) + 8, 4));
  return getValueFromBytes(superblockBuffer, 2048 + (groupIndex * 32) + 8, 4);
}

unsigned int FileSystem::getBlockStartingByte(int blockNumber) {
  return blockNumber *BLOCK_SIZE;
}

unsigned int FileSystem::getBlockSector(partition_entry *partition,
                                          unsigned int blockNumber) {
  return (partition->startSector + (blockNumber * (BLOCK_SIZE / SECTOR_SIZE_BYTES)));
}

unsigned int FileSystem::getInodeStartingByte(unsigned int inode_no) {
  return (BLOCK_SIZE * getInodeTableBlockNumber(inode_no))
      + (super_block.s_inode_size
          * ((inode_no - 1) % super_block.s_inodes_per_group));
}

void FileSystem::scanDirectoryBlock(partition_entry *partition,
                                     unsigned int blockNumber) {
  unsigned char buf[BLOCK_SIZE];
  unsigned int i = 0, j;
  readSectors(getBlockSector(partition, blockNumber), 2, buf);
  while (i <BLOCK_SIZE - 1) {
    struct ext2_dir_entry_2 fileEntry;
    fileEntry.inode = (__u32 ) getValueFromBytes(buf, i + 0, 4);
    fileEntry.rec_len = (__u16 ) getValueFromBytes(buf, i + 4, 2);
    fileEntry.name_len = (__u8 ) buf[i + 6];
    fileEntry.file_type = (__u8 ) buf[i + 7];
    printf("inode, rec_len, name_len, file_type: %d, %d, %d, %d\n",
           fileEntry.inode, fileEntry.rec_len, fileEntry.name_len,
           fileEntry.file_type);
    for (j = 0; j < fileEntry.name_len; j++) {
      fileEntry.name[j] = buf[i + 8 + j];
    }
    fileEntry.name[fileEntry.name_len] = '\0';
    printf("Name: %s\n", fileEntry.name);
    i = i + fileEntry.rec_len;
  }
}

/*
 *
 */
void FileSystem::parseFilesystem(partition_entry *partition,
                                  unsigned int blockNumber) {
  unsigned char buf[BLOCK_SIZE];
  unsigned int i = 0, j;
  readSectors(getBlockSector(partition, blockNumber), 2, buf);
  while (i <BLOCK_SIZE - 1) {
    struct ext2_dir_entry_2 fileEntry;
    fileEntry.inode = (__u32 ) getValueFromBytes(buf, i + 0, 4);
    if (fileEntry.inode == 0)
      return;
    fileEntry.rec_len = (__u16 ) getValueFromBytes(buf, i + 4, 2);
    fileEntry.name_len = (__u8 ) buf[i + 6];
    fileEntry.file_type = (__u8 ) buf[i + 7];
    printf("inode, rec_len, name_len, file_type: %d, %d, %d, %d\n",
           fileEntry.inode, fileEntry.rec_len, fileEntry.name_len,
           fileEntry.file_type);
    for (j = 0; j < fileEntry.name_len; j++) {
      fileEntry.name[j] = buf[i + 8 + j];
    }
    fileEntry.name[fileEntry.name_len] = '\0';
    i = i + fileEntry.rec_len;
    if (strcmp(fileEntry.name, ".") != 0
        && strcmp(fileEntry.name, "..") != 0) {
      inode_data inode = readInode(partition, fileEntry.inode);
      if (!(inode.file_type & EXT2_S_IFDIR) == 0) {
        readDataBlocks(partition, inode.pointers_data_block);
      }
    }
    printf("\nEnd of %s\n\n", fileEntry.name);
  }
}

void FileSystem::readIndirectDataBlocks(partition_entry *partition,
                                           unsigned int blockNumber,
                                           unsigned int indirectionLevel) {
  unsigned int i = 0;
  unsigned char buf[BLOCK_SIZE];
  unsigned int sector = getBlockSector(partition, blockNumber);
  readSectors(sector, 2, buf);
  for (i = 0; i <BLOCK_SIZE; i += 4) {
    if (indirectionLevel == 3 || indirectionLevel == 2)
      readIndirectDataBlocks(partition, getValueFromBytes(buf, i, 4),
                                indirectionLevel - 1);
    else if (indirectionLevel == 1)
      parseFilesystem(partition, blockNumber);
  }
}

void FileSystem::readDataBlocks(partition_entry *partition,
                                  unsigned int *pointers) {
  int i = 0;
  for (i = 0; i < 12; i++) {
    if (pointers[i] != 0)
      parseFilesystem(partition, pointers[i]);
  }
  if (pointers[12] != 0)
    readIndirectDataBlocks(partition, pointers[12], 1);
  if (pointers[13] != 0)
    readIndirectDataBlocks(partition, pointers[13], 2);
  if (pointers[14] != 0)
    readIndirectDataBlocks(partition, pointers[14], 3);
}

inode_data FileSystem::readInode(partition_entry *partition,
                                  unsigned int inode_no) {
  inode_data inode;
  unsigned char buf[BLOCK_SIZE];
  int i;
  int inodeOffset = getInodeStartingByte(inode_no);
  int inodeSector = getBlockSector(partition, inodeOffset /BLOCK_SIZE);
  int temp = inodeOffset
      - ((inodeSector - partition->startSector) * SECTOR_SIZE_BYTES);
  readSectors(inodeSector, 2, buf);
  inode.inode_no = inode_no;
  inode.file_type = getValueFromBytes(buf, temp + 0, 2);
  inode.file_length = getValueFromBytes(buf, temp + 4, 4);
  inode.no_data_blocks = getValueFromBytes(buf, temp + 28, 4);
  for (i = 0; i < 15; i++) {
    inode.pointers_data_block[i] = getValueFromBytes(buf, temp + 40 + (i * 4),
                                                     4);
  }
  return inode;
}

int FileSystem::checkInodeBitmap(partition_entry *partition,
                                   unsigned int inode_no) {
  unsigned char inode_bitmap[1024];
  unsigned int byte = inode_no / 8;
  unsigned int offset = 7 - (inode_no % 8);
  return !(!(inode_bitmap[byte] & (1 << offset)));
}

int FileSystem::checkBlockBitmap(partition_entry *partition,
                                   unsigned int blockNumber) {
  if (blockNumber == 0)
    return 0;
  unsigned char blockBitmap[1024];
  unsigned int groupIndex = (blockNumber - 1) / super_block.s_blocks_per_group;
  unsigned int blockOffset = (blockNumber - 1) % super_block.s_blocks_per_group;
  unsigned int blockBitmapSector = getBlockSector(
      partition,
      getValueFromBytes(superblockBuffer, 2048 + (groupIndex * 32) + 0, 4));
  readSectors(blockBitmapSector, 1, blockBitmap);
  unsigned int byte = blockOffset / 8;
  unsigned int offset = 7 - (blockOffset % 8);
  return !(!(blockBitmap[byte] & (1 << offset)));
}

int FileSystem::getIndirectDataBlockQt(partition_entry *partition,
                                              unsigned int blockNumber,
                                              unsigned int indirectionLevel) {
  int count = 0, i = 0;
  unsigned char buf[BLOCK_SIZE];
  unsigned int sector = getBlockSector(partition, blockNumber);
  readSectors(sector, 2, buf);
  for (i = 0; i < 1024; i += 4) {
    if (indirectionLevel == 3 || indirectionLevel == 2)
      count += getIndirectDataBlockQt(partition,
                                             getValueFromBytes(buf, i, 4),
                                             indirectionLevel - 1);
    else if (indirectionLevel == 1)
      count += checkBlockBitmap(partition, getValueFromBytes(buf, i, 4));
  }
  return count;
}

int FileSystem::getDataBlockQt(partition_entry *partition,
                                     unsigned int *pointers) {
  int count = 0, i = 0;
  for (i = 0; i < 12; i++) {
    if (checkBlockBitmap(partition, pointers[i]))
      count++;
  }
  if (pointers[12] != 0)
    count += getIndirectDataBlockQt(partition, pointers[12], 1);
  if (pointers[13] != 0)
    count += getIndirectDataBlockQt(partition, pointers[13], 2);
  if (pointers[14] != 0)
    count += getIndirectDataBlockQt(partition, pointers[14], 3);
  return count;
}

void FileSystem::readSuperblock(partition_entry *partition) {
  readSectors(partition->startSector, 6, superblockBuffer);
  printf("Signature: 0x%02X 0x%02X\n", superblockBuffer[1080],
         superblockBuffer[1081]);
  super_block.s_magic = getValueFromBytes(superblockBuffer, 1080, 2);
  printf("Inodes: %d\n", getValueFromBytes(superblockBuffer, 1024, 4));
  super_block.s_inodes_count = getValueFromBytes(superblockBuffer, 1024, 4);
  printf("File system size: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 4, 4));
  printf("Reserved blocks: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 8, 4));
  super_block.s_r_blocks_count = getValueFromBytes(superblockBuffer, 1024 + 8, 4);
  printf("Free blocks: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 12, 4));
  super_block.s_free_blocks_count = getValueFromBytes(superblockBuffer, 1024 + 12,
                                                      4);
  printf("Free inodes: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 16, 4));
  super_block.s_free_inodes_count = getValueFromBytes(superblockBuffer, 1024 + 16,
                                                      4);
  printf("Blocks per group: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 32, 4));
  super_block.s_blocks_per_group = getValueFromBytes(superblockBuffer, 1024 + 32,
                                                     4);
  printf("Inodes per group: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 40, 4));
  super_block.s_inodes_per_group = getValueFromBytes(superblockBuffer, 1024 + 40,
                                                     4);
  printf("First useful block: 0x%02X 0x%02X 0x%02x 0x%02x\n",
         superblockBuffer[1044], superblockBuffer[1045], superblockBuffer[1046],
         superblockBuffer[1047]);
  printf("Block size: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 24, 4));
  super_block.s_log_block_size = getValueFromBytes(superblockBuffer, 1024 + 24,
                                                   4);
  printf("Size of on disk inode structure: %d\n",
         getValueFromBytes(superblockBuffer, 1024 + 88, 2));
  super_block.s_inode_size = getValueFromBytes(superblockBuffer, 1024 + 88, 2);
  printf("Block number of the block bitmap: %d\n",
         getValueFromBytes(superblockBuffer, 2048 + 0, 4));
  printf("Block number of the inode bitmap: %d\n",
         getValueFromBytes(superblockBuffer, 2048 + 4, 4));
  printf("First inode table block and starting byte: %d, %d\n",
         getValueFromBytes(superblockBuffer, 2048 + 8, 4),
         getInodeStartingByte(1));
}

void FileSystem::readRootInode(partition_entry *partition) {
  inode_data inode = readInode(partition, 2);
  unsigned int firstDataBlock = inode.pointers_data_block[0];
  printf("First data block: %d\n", firstDataBlock);
  unsigned int firstDataSector = getBlockSector(partition,
                                                    firstDataBlock);
  unsigned char dataBuffer[2 * SECTOR_SIZE_BYTES];
  readSectors(firstDataSector, 2, dataBuffer);
  struct ext2_dir_entry rootDirectory;
  rootDirectory.inode = (__u32 ) getValueFromBytes(dataBuffer, 0, 4);
  rootDirectory.rec_len = (__u16 ) getValueFromBytes(dataBuffer, 4, 2);
  rootDirectory.name_len = (__u16 ) dataBuffer[6];
  unsigned int i;
  for (i = 0; i < rootDirectory.name_len; i++) {
    rootDirectory.name[rootDirectory.name_len - 1 - i] = dataBuffer[8 + i];
  }
  rootDirectory.name[rootDirectory.name_len] = '\0';
  readDataBlocks(partition, inode.pointers_data_block);
}

FileSystem::~FileSystem() {
  close(device);
}
