#include "FileSystem.h"

FileSystem::FileSystem(char* diskImage) {
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
        | (((int) partitionBuffer[9] & 0xFF) << 8) | (((int) partitionBuffer[8]) & 0xFF))
        + extBootRecordOffset;
  else
    entry->startSector = ((((int) partitionBuffer[11] & 0xFF) << 24)
        | (((int) partitionBuffer[10] & 0xFF) << 16)
        | (((int) partitionBuffer[9] & 0xFF) << 8) | (((int) partitionBuffer[8]) & 0xFF))
        + sectorOffset;

  entry->length = (((int) partitionBuffer[15] & 0xFF) << 24)
      | (((int) partitionBuffer[14] & 0xFF) << 16) | (((int) partitionBuffer[13] & 0xFF) << 8)
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
  for (i = partitionAddress; i < partitionAddress + PARTITION_RECORD_SIZE; i++) {
    partitionBuffer[i - partitionAddress] = buf[i];
  }

  partition_entry *part = readPartitionEntry(partitionBuffer, partitionNumber,
                                               sectorOffset);

  return part;
}

partition_entry* FileSystem::getPartitionTable(int sector,
                                                    int sectorOffset) {

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
      entry->next = getPartitionTable(temp->startSector,
                                           temp->startSector);
      partition_entry *cur = entry->next;
      partition_entry *prev = entry;
      while (cur != NULL) {
        if (cur->type == 5 || cur->type == 0) {
          if (cur->next == NULL) {
            free(cur);
            prev->next = NULL;
          } else {
            prev->next = cur->next;
            free(cur);
            cur = prev->next;
            continue;
          }
        }
        prev = cur;
        cur = cur->next;
      }
    }
    while (entry->next != NULL)
      entry = entry->next;
    temp = temp->next;
  }

  return first;
}

FileSystem::~FileSystem() {
  close(device);
}
