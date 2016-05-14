#include "filesystem.h"

FileSystem::FileSystem(const char* diskImage, bool performRepair, QTextBrowser *textBrowser, QProgressBar *progressBar) {
  if ((device = open(diskImage, O_RDWR)) == -1) {
      textBrowser->clear();
      textBrowser->append("Device not found.");
    return;
  }
  errorQt = 0;
  extBootRecordOffset = 0;
  lostFoundInode = -1;
  firstRootBataBlock = -1;
  blockSize = 1024;
  inodeMap = NULL;
  inodeLinkCount = NULL;
  blockMap = NULL;
  this->performRepair = performRepair;
  textBrowser->clear();
  this->textBrowser = textBrowser;
  this->progressBar = progressBar;
  startFileSystemChecking();
}

int FileSystem::startFileSystemChecking() {
  PartitionEntry *entry = getPartitionTable(0, 0);
  progressBar->setValue(10);
  PartitionEntry *temp = entry;
  while (temp != NULL) {
    if (temp->type == 0x83) {
      readSuperblock(temp);
      setInfo();
      readRootInode(temp);
      freeInfo();
    }
    temp = temp->next;
    progressBar->setValue(progressBar->value() + 30);
  }
  progressBar->setValue(100);
      textBrowser->append(QString::number(errorQt) + " errors were found. ");
  if(performRepair){
      textBrowser->append("Filesystem has been successfully checked and repaired.");
  } else {
      textBrowser->append("Filesystem has been successfully checked. For repair uncheck box under start button.");
  }
  return 0;
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

void FileSystem::writeSectors(int64_t startSector, unsigned int sectorsQt,
                              void *from) {
  ssize_t ret;
  int64_t lret;
  int64_t sectorOffset;
  ssize_t bytesToWrite;

  sectorOffset = startSector * SECTOR_SIZE_BYTES;

  if ((lret = lseek64(device, sectorOffset, SEEK_SET)) != sectorOffset) {
    exit(-1);
  }

  bytesToWrite = SECTOR_SIZE_BYTES * sectorsQt;

  if ((ret = write(device, from, bytesToWrite)) != bytesToWrite) {
    exit(-1);
  }
}

PartitionEntry* FileSystem::readPartitionEntry(unsigned char *partitionBuffer,
                                               int partitionNumber,
                                               int sectorOffset) {

  int type = (int) partitionBuffer[4] & 0xFF;
  if (type != 0x82 && type != 0x00 && type != 0x83 && type != 0x05) {
    return NULL;
  }

  PartitionEntry *entry = new PartitionEntry;
  entry->partitionNumber = partitionNumber;
  entry->type = type;

  if (type == 0x05) {
    entry->startSector = ((((int) partitionBuffer[11] & 0xFF) << 24)
        | (((int) partitionBuffer[10] & 0xFF) << 16)
        | (((int) partitionBuffer[9] & 0xFF) << 8)
        | (((int) partitionBuffer[8]) & 0xFF)) + extBootRecordOffset;
  } else {
    entry->startSector = ((((int) partitionBuffer[11] & 0xFF) << 24)
        | (((int) partitionBuffer[10] & 0xFF) << 16)
        | (((int) partitionBuffer[9] & 0xFF) << 8)
        | (((int) partitionBuffer[8]) & 0xFF)) + sectorOffset;
  }

  entry->length = (((int) partitionBuffer[15] & 0xFF) << 24)
      | (((int) partitionBuffer[14] & 0xFF) << 16)
      | (((int) partitionBuffer[13] & 0xFF) << 8)
      | ((int) partitionBuffer[12] & 0xFF);

  entry->next = NULL;

  return entry;
}

PartitionEntry* FileSystem::readPartitionTable(int sector, int partitionNumber,
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

  PartitionEntry *part = readPartitionEntry(partitionBuffer, partitionNumber,
                                            sectorOffset);

  return part;
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

unsigned int FileSystem::getInodeTableBlockNumber(unsigned int inodeNumber) {
  unsigned int groupIndex = (inodeNumber - 1) / super_block.s_inodes_per_group;
  return getValueFromBytes(superblockBuffer,
                           1024 + blockSize + (groupIndex * 32) + 8, 4);
}

unsigned int FileSystem::getBlockStartingByte(int blockNumber) {
  return blockNumber * blockSize;
}

unsigned int FileSystem::getBlockSector(PartitionEntry *partition,
                                        unsigned int blockNumber) {
  return (partition->startSector
      + (blockNumber * (blockSize / SECTOR_SIZE_BYTES)));
}

unsigned int FileSystem::getInodeStartingByte(unsigned int inodeNumber) {
  return (blockSize * getInodeTableBlockNumber(inodeNumber))
      + (super_block.s_inode_size
          * ((inodeNumber - 1) % super_block.s_inodes_per_group));
}

void FileSystem::writeInodeEntry(PartitionEntry *partition,
                                 unsigned int inodeNumber) {
  InodeData inode = readInode(partition, lostFoundInode);
  InodeData inode_cur = readInode(partition, inodeNumber);
  int blockNumber = readDataBlocks(partition, lostFoundInode,
                                   (blockSize / SECTOR_SIZE_BYTES),
                                   inode.dataBlocksPointers, 0, 0, 2);
  if (blockNumber == -1)
    return;
  unsigned int blockSector = getBlockSector(partition, blockNumber);
  unsigned char buf[blockSize];
  unsigned int i = 0;
  readSectors(blockSector, (blockSize / SECTOR_SIZE_BYTES), buf);
  while (i < blockSize - 1) {
    struct ext2_dir_entry_2 *fileEntry = (struct ext2_dir_entry_2 *) (buf + i);
    if (fileEntry->rec_len
        > ((((__u16 ) 8 + fileEntry->name_len) + 3) & ~0x03)) {
      fileEntry->rec_len = (((__u16 ) 8 + fileEntry->name_len) + 3) & ~0x03;
      i = i + fileEntry->rec_len;
      fileEntry = (struct ext2_dir_entry_2*) (buf + i);
      fileEntry->inode = inodeNumber;
      sprintf(fileEntry->name, "%d", inodeNumber);
      fileEntry->rec_len = (__u16 ) (blockSize - i);
      fileEntry->name_len = (__u8 ) (strlen(fileEntry->name));
      if (!(inode_cur.fileType & EXT2_S_IFREG) == 0)
        fileEntry->file_type = 1;
      else if (!(inode_cur.fileType & EXT2_S_IFDIR) == 0)
        fileEntry->file_type = 2;
      writeSectors(blockSector, (blockSize / SECTOR_SIZE_BYTES), buf);
      return;
    } else {
      i = i + fileEntry->rec_len;
    }
  }
}

unsigned int FileSystem::parseFilesystem(PartitionEntry *partition,
                                         unsigned int blockNumber,
                                         unsigned int passNumber,
                                         unsigned int currentInode,
                                         unsigned int parentInode,
                                         int performCheck) {
  unsigned char buf[blockSize];
  unsigned int i = 0;
  struct ext2_dir_entry_2 *fileEntry;
  unsigned int blockSector = getBlockSector(partition, blockNumber);
  readSectors(blockSector, (blockSize / SECTOR_SIZE_BYTES), buf);
  while (i < blockSize - 1) {

    fileEntry = (struct ext2_dir_entry_2 *) (buf + i);

    if (fileEntry->inode == 0)
      return -1;

    if (currentInode == 2 && parentInode == 2
        && (strcmp(fileEntry->name, "lost+found") == 0)) {
      lostFoundInode = fileEntry->inode;
    }

    if (passNumber == 1 && performCheck == 1) {
      if (fileEntry->inode != currentInode
          && (strcmp(fileEntry->name, ".") == 0)) {
        QString tmpStr = QString("Entry '.' has inode ") + QString::number(fileEntry->inode) + QString(" instead of ") + QString::number(currentInode) + QString(".\n") ;
        textBrowser->append(tmpStr);
        errorQt++;
        fileEntry->inode = currentInode;
        writeSectors(blockSector, (blockSize / SECTOR_SIZE_BYTES), buf);
      }
      if (fileEntry->inode != parentInode && (!strcmp(fileEntry->name, ".."))) {
        QString tmpStr = QString("Entry '..' has inode ") + QString::number(fileEntry->inode) + QString(" instead of ") + QString::number(parentInode) + QString(".\n") ;
        textBrowser->append(tmpStr);
        errorQt++;
        fileEntry->inode = parentInode;
        writeSectors(blockSector, (blockSize / SECTOR_SIZE_BYTES), buf);
      }
    } else if (passNumber == 2 && performCheck == 1) {
      inodeMap[fileEntry->inode] = 1;
    } else if (passNumber == 3 && performCheck == 1) {
      inodeLinkCount[fileEntry->inode] += 1;
    } else if (passNumber == 4 && performCheck == 1) {
      blockMap[blockNumber] = 1;
    }

    if (strcmp(fileEntry->name, ".") && strcmp(fileEntry->name, "..")
        && performCheck != 0) {
      if (performCheck == 2) {
        inodeMap[fileEntry->inode] = 1;
      }
      InodeData inode = readInode(partition, fileEntry->inode);
      if ((inode.fileType & 0xF000) == EXT2_S_IFREG && passNumber == 4) {
        readDataBlocks(partition, fileEntry->inode, currentInode,
                       inode.dataBlocksPointers, passNumber, performCheck, 1);
      } else if (!(inode.fileType & EXT2_S_IFDIR) == 0) {
        readDataBlocks(partition, fileEntry->inode, currentInode,
                       inode.dataBlocksPointers, passNumber, performCheck, 2);
      }
    }
    i = i + fileEntry->rec_len;
  }
  if (fileEntry != NULL && fileEntry->rec_len > 8 + fileEntry->name_len
      && (fileEntry->rec_len - 8 - fileEntry->name_len) > 16 && !performCheck)
    return blockNumber;
  return -1;
}

unsigned int FileSystem::readIndirectDataBlocks(PartitionEntry *partition,
                                                unsigned int inode,
                                                unsigned int parentInode,
                                                unsigned int blockNumber,
                                                unsigned int indirectionLevel,
                                                int passNumber,
                                                int performCheck,
                                                int fileType) {
  unsigned int i = 0;
  int retValue = -1;
  unsigned char buf[blockSize];
  unsigned int sector = getBlockSector(partition, blockNumber);
  readSectors(sector, (blockSize / SECTOR_SIZE_BYTES), buf);
  for (i = 0; i < blockSize; i += 4) {
    unsigned int block = getValueFromBytes(buf, i, 4);
    if (block != 0 && (indirectionLevel == 3 || indirectionLevel == 2)) {
      if (passNumber == 4)
        blockMap[block] = 1;
      retValue = readIndirectDataBlocks(partition, inode, parentInode, block,
                                        indirectionLevel - 1, passNumber,
                                        performCheck, fileType);
    } else if (indirectionLevel == 1 && block != 0) {
      if (fileType != 1)
        retValue = parseFilesystem(partition, block, passNumber, inode,
                                   parentInode, performCheck);
      if (passNumber == 4) {
        blockMap[block] = 1;
        continue;
      }
      if (retValue != -1 && !performCheck)
        return retValue;
    }
    if (retValue != -1 && !performCheck)
      return retValue;
  }
  return -1;
}

unsigned int FileSystem::readDataBlocks(PartitionEntry *partition,
                                        unsigned int inode,
                                        unsigned int parentInode,
                                        unsigned int *pointers, int passNumber,
                                        int performCheck, int fileType) {
  int i = 0, retValue = -1;
  for (i = 0; i < 12; i++) {
    if (pointers[i] != 0) {
      blockMap[pointers[i]] = 1;
      if (fileType != 1)
        retValue = parseFilesystem(partition, pointers[i], passNumber, inode,
                                   parentInode, performCheck);
      if (passNumber == 4)
        continue;
    }
    if (!performCheck && retValue != -1)
      return retValue;
  }
  if (pointers[12] != 0)
    retValue = readIndirectDataBlocks(partition, inode, parentInode,
                                      pointers[12], 1, passNumber, performCheck,
                                      fileType);
  if (!performCheck && retValue != -1)
    return retValue;

  if (pointers[13] != 0)
    retValue = readIndirectDataBlocks(partition, inode, parentInode,
                                      pointers[13], 2, passNumber, performCheck,
                                      fileType);
  if (!performCheck && retValue != -1)
    return retValue;

  if (pointers[14] != 0)
    retValue = readIndirectDataBlocks(partition, inode, parentInode,
                                      pointers[14], 3, passNumber, performCheck,
                                      fileType);
  return retValue;
}

void FileSystem::UpdateHardLinkCounter(PartitionEntry *partition,
                                       unsigned int inodeNumber,
                                       unsigned int hardLinkQt) {
  unsigned char buf[blockSize];
  int inodeOffset = getInodeStartingByte(inodeNumber);
  int inodeSector = getBlockSector(partition, inodeOffset / blockSize);
  int temp = inodeOffset
      - ((inodeSector - partition->startSector) * SECTOR_SIZE_BYTES);
  readSectors(inodeSector, (blockSize / SECTOR_SIZE_BYTES), buf);
  struct ext2_inode *inode = (struct ext2_inode *) (buf + temp);
  inode->i_links_count = (__u16 ) hardLinkQt;
  writeSectors(inodeSector, (blockSize / SECTOR_SIZE_BYTES), buf);
}

InodeData FileSystem::readInode(PartitionEntry *partition,
                                unsigned int inodeNumber) {
  InodeData inode;
  unsigned char buf[blockSize];
  int i;
  int inodeOffset = getInodeStartingByte(inodeNumber);
  int inodeSector = getBlockSector(partition, inodeOffset / blockSize);
  int temp = inodeOffset
      - ((inodeSector - partition->startSector) * SECTOR_SIZE_BYTES);
  readSectors(inodeSector, (blockSize / SECTOR_SIZE_BYTES), buf);
  //First data block
  inode.inodeNumber = inodeNumber;
  inode.fileType = getValueFromBytes(buf, temp + 0, 2);
  inode.fileLength = getValueFromBytes(buf, temp + 4, 4);
  inode.hardLinksQt = getValueFromBytes(buf, temp + 26, 2);
  inode.dataBlocksQt = getValueFromBytes(buf, temp + 28, 4);
  for (i = 0; i < 15; i++) {
    inode.dataBlocksPointers[i] = getValueFromBytes(buf, temp + 40 + (i * 4),
                                                    4);
  }
  return inode;
}

int FileSystem::checkInodeBitmap(PartitionEntry *partition,
                                 unsigned int inodeNumber) {
  if (inodeNumber == 0)
    return 0;
  unsigned char inodeBitmap[blockSize];
  unsigned int groupIndex = (inodeNumber - 1) / super_block.s_inodes_per_group;
  unsigned int inodeOffset = (inodeNumber - 1) % super_block.s_inodes_per_group;
  unsigned int inodeBitmapSector = getBlockSector(
      partition,
      getValueFromBytes(superblockBuffer,
                        1024 + blockSize + (groupIndex * 32) + 4, 4));
  readSectors(inodeBitmapSector, blockSize / SECTOR_SIZE_BYTES, inodeBitmap);
  unsigned int byte = inodeOffset / 8;
  unsigned int offset = (inodeOffset % 8);
  return !(!(inodeBitmap[byte] & (1 << offset)));
}

int FileSystem::checkBlockBitmap(PartitionEntry *partition,
                                 unsigned int blockNumber) {
  if (blockNumber == 0)
    return 0;
  unsigned char blockBitmap[blockSize];
  unsigned int groupIndex = (blockNumber - 1) / super_block.s_blocks_per_group;
  unsigned int blockOffset = (blockNumber - 1) % super_block.s_blocks_per_group;
  unsigned int blockBitmapSector = getBlockSector(
      partition,
      getValueFromBytes(superblockBuffer,
                        1024 + blockSize + (groupIndex * 32) + 0, 4));
  readSectors(blockBitmapSector, (blockSize / SECTOR_SIZE_BYTES), blockBitmap);
  unsigned int byte = blockOffset / 8;
  unsigned int offset = (blockOffset % 8);
  return !(!(blockBitmap[byte] & (1 << offset)));
}

void FileSystem::setBlockBitmap(PartitionEntry *partition,
                                unsigned int blockNumber, int value) {
  if (blockNumber == 0)
    return;
  unsigned char blockBitmap[blockSize];
  unsigned int groupIndex = (blockNumber - 1) / super_block.s_blocks_per_group;
  unsigned int blockOffset = (blockNumber - 1) % super_block.s_blocks_per_group;
  unsigned int blockBitmapSector = getBlockSector(
      partition,
      getValueFromBytes(superblockBuffer,
                        1024 + blockSize + (groupIndex * 32) + 0, 4));
  readSectors(blockBitmapSector, (blockSize / SECTOR_SIZE_BYTES), blockBitmap);
  unsigned int byte = blockOffset / 8;
  unsigned int offset = (blockOffset % 8);
  blockBitmap[byte] |= (1 << offset);
  writeSectors(blockBitmapSector, (blockSize / SECTOR_SIZE_BYTES), blockBitmap);
}

int FileSystem::getIndirectDataBlockQt(PartitionEntry *partition,
                                       unsigned int blockNumber,
                                       unsigned int indirectionLevel) {
  int count = 0, i = 0;
  unsigned char buf[blockSize];
  unsigned int sector = getBlockSector(partition, blockNumber);
  readSectors(sector, 2, buf);
  for (i = 0; i < 1024; i += 4) {
    if (indirectionLevel == 3 || indirectionLevel == 2)
      count += getIndirectDataBlockQt(partition, getValueFromBytes(buf, i, 4),
                                      indirectionLevel - 1);
    else if (indirectionLevel == 1)
      count += checkBlockBitmap(partition, getValueFromBytes(buf, i, 4));
  }
  return count;
}

int FileSystem::getDataBlockQt(PartitionEntry *partition,
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

void FileSystem::readSuperblock(PartitionEntry *partition) {
  readSectors(partition->startSector, 6, superblockBuffer);
  super_block.s_magic = getValueFromBytes(superblockBuffer, 1080, 2);
  super_block.s_inodes_count = getValueFromBytes(superblockBuffer, 1024, 4);
  super_block.s_blocks_count = getValueFromBytes(superblockBuffer, 1024 + 4, 4);
  super_block.s_r_blocks_count = getValueFromBytes(superblockBuffer, 1024 + 8,
                                                   4);
  super_block.s_free_blocks_count = getValueFromBytes(superblockBuffer,
                                                      1024 + 12, 4);
  super_block.s_free_inodes_count = getValueFromBytes(superblockBuffer,
                                                      1024 + 16, 4);
  super_block.s_blocks_per_group = getValueFromBytes(superblockBuffer,
                                                     1024 + 32, 4);
  super_block.s_inodes_per_group = getValueFromBytes(superblockBuffer,
                                                     1024 + 40, 4);
  super_block.s_log_block_size = getValueFromBytes(superblockBuffer, 1024 + 24,
                                                   4);
  super_block.s_inode_size = getValueFromBytes(superblockBuffer, 1024 + 88, 2);
}

void FileSystem::readRootInode(PartitionEntry *partition) {
  unsigned int i;
  InodeData inode = readInode(partition, 2);
  blockSize = 1 << (super_block.s_log_block_size + 10);
  firstRootBataBlock = inode.dataBlocksPointers[0];
  readDataBlocks(partition, 2, 2, inode.dataBlocksPointers, 1, 1, 2);
  readDataBlocks(partition, 2, 2, inode.dataBlocksPointers, 2, 1, 2);
  for (i = 11; i <= super_block.s_inodes_count; i++) {
    InodeData in = readInode(partition, i);
    if (!(in.fileType & EXT2_S_IFDIR) == 0
        && checkInodeBitmap(partition, i) == 1) {
      readDataBlocks(partition, i, -1, in.dataBlocksPointers, 0, 2, 2);
    }
  }
  for (i = 11; i <= super_block.s_inodes_count; i++) {
    unsigned int bitmapValue = checkInodeBitmap(partition, i);
    if (bitmapValue == 1 && inodeMap[i] == 0) {
      InodeData in = readInode(partition, i);
      if (in.fileType != 0) {
        QString tmpStr = QString("Inode ") + QString::number(i) + QString(" has invalid entry in inode bitmap. Bitmap value: ") + QString::number(bitmapValue) + QString(", collected value:") + QString::number(inodeMap[i]) + QString(".\n");
        textBrowser->append(tmpStr);
        errorQt++;
        if (performRepair) {
          writeInodeEntry(partition, i);
        }
      }
    }
  }
  readDataBlocks(partition, 2, 2, inode.dataBlocksPointers, 1, 1, 2);
  readDataBlocks(partition, 2, 2, inode.dataBlocksPointers, 3, 1, 2);
  for (i = 1; i <= super_block.s_inodes_count; i++) {
    InodeData in = readInode(partition, i);
    if (inodeLinkCount[i] != in.hardLinksQt) {
      if (in.fileType != 0) {
        QString tmpStr = QString("Inode ") + QString::number(i) + QString(" has invalid inode count in inode entry. Current value : ") + QString::number(in.hardLinksQt) + QString(", collected value:") + QString::number(inodeLinkCount[i]) + QString(".\n");
        textBrowser->append(tmpStr);
        errorQt++;
      }
      if (performRepair) {
        UpdateHardLinkCounter(partition, i, inodeLinkCount[i]);
      }
    }
  }
  readDataBlocks(partition, 2, 2, inode.dataBlocksPointers, 4, 1, 2);
  for (i = 1; i <= super_block.s_blocks_count; i++) {
    unsigned int bitmapValue = checkBlockBitmap(partition, i);
    if (i < firstRootBataBlock && checkBlockBitmap(partition, i) == 0) {
      QString tmpStr = QString("Block ") + i + QString(" has invalid entry in block bitmap. Bitmap value: 0, collected value: 1.\n");
      textBrowser->append(tmpStr);
      errorQt++;
      if (performRepair) {
        setBlockBitmap(partition, i, 1);
      }
    }
    if (blockMap[i] == 1 && bitmapValue != blockMap[i]) {
      QString tmpStr = QString("Block ")+ QString::number(i) + QString(" has invalid entry in block bitmap. Bitmap value: ") + QString::number(bitmapValue) + QString(", collected value: ") + QString::number(blockMap[i]) + QString(".\n");
      textBrowser->append(tmpStr);
      errorQt++;
      if (performRepair) {
        setBlockBitmap(partition, i, 1);
      }
    }
  }
}

PartitionEntry* FileSystem::getPartitionTable(int sector, int sectorOffset) {

  int i;
  PartitionEntry *entry = NULL;
  PartitionEntry *first = NULL;

  int partitionCount = 4;
  if (sectorOffset != 0)
    partitionCount = 2;

  for (i = 1; i <= partitionCount; i++) {
    PartitionEntry *temp = readPartitionTable(sector, i, sectorOffset);
    if (entry == NULL) {
      entry = temp;
      first = entry;
    } else if (temp != NULL) {
      entry->next = temp;
      entry = entry->next;
    }
  }

  PartitionEntry *temp = first;
  PartitionEntry *end = entry;
  while (temp != end->next) {
    if (temp->type == 5) {
      if (extBootRecordOffset == 0)
        extBootRecordOffset = temp->startSector;
      entry->next = getPartitionTable(temp->startSector, temp->startSector);
      PartitionEntry *current = entry->next;
      PartitionEntry *prev = entry;
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

PartitionEntry* FileSystem::getPartitionEntry(PartitionEntry *head,
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

void FileSystem::setInfo() {
  inodeMap = (unsigned int *) calloc(super_block.s_inodes_count,
                                     sizeof(unsigned int));
  inodeLinkCount = (unsigned int *) calloc(super_block.s_inodes_count,
                                           sizeof(unsigned int));
  blockMap = (unsigned int *) calloc(super_block.s_blocks_count,
                                     sizeof(unsigned int));
}

void FileSystem::freeInfo() {
  free(inodeMap);
  free(inodeLinkCount);
  free(blockMap);
}

FileSystem::~FileSystem() {
  close(device);
}
