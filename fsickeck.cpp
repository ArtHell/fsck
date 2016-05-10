/**
 * FSICheck 2.0.0
 * In this version added functions to show the content of the file system
 */

#include "FileSystem.h"

int main(int argc, char **argv) {
  char *disk_image = "disk"; // will be entered by user
  FileSystem* ext2 = new FileSystem(disk_image);
  partition_entry *entry = ext2->getPartitionTable(0, 0);
  ext2->readSuperblock(entry);
  ext2->readRootInode(entry);
  delete ext2;
  return 0;
}

