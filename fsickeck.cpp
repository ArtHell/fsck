/**
 * FSICheck 3.0.0
 * In this version added added checking and repairing algorithm
 */

#include "FileSystem.h"

int main(int argc, char **argv) {
  if(argc < 2) {
    cout << "Usage: ./fsicheck <path to disk>" << endl;
    return 0;
  }

  bool performRepair;
  cout << "Perform repair? (y/n)";
  char chooser;
  cin >> chooser;
  if(chooser == 'y') {
    performRepair = true;
  } else {
    performRepair = false;
  }
  FileSystem* ext2 = new FileSystem(argv[1], performRepair);
  ext2->startFileSystemChecking();
  delete ext2;
  printf("End of checking.");
  return 0;
}

