/**
 * FSICheck 2.0.0
 * In this version added functions to show the content of the file system
 */

#include "FileSystem.h"

int main(int argc, char **argv) {
  char *disk_image = "disk";  // will be entered by user

  FileSystem* ext2 = new FileSystem(disk_image);
  ext2->startFileSystemChecking();
  delete ext2;
  printf("End of checking. Partitions has been repaired.");
  return 0;
}

