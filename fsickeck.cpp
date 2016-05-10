/**
 * FSICheck 1.0.0
 * This version just show partitions of disk.
 */

#include "FileSystem.h"

int main(int argc, char **argv) {
  char *disk_image = "disk"; // will be entered by user
  FileSystem* ext2 = new FileSystem(disk_image);
  partition_entry *entry = ext2->getPartitionTable(0, 0);
  delete ext2;

  while (entry != NULL) {
    printf("0x%02X %d %d\n", entry->type, entry->startSector, entry->length);
    entry = entry->next;
  }
  return 0;
}

