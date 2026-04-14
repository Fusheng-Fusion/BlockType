#include "blocktype/Config/Version.h"
#include "llvm/Support/raw_ostream.h"

int main(int argc, char *argv[]) {
  if (argc > 1 && llvm::StringRef(argv[1]) == "--version") {
    llvm::outs() << "blocktype version " << BLOCKTYPE_VERSION_STRING << "\n";
    return 0;
  }
  
  llvm::outs() << "BlockType - A C++26 compiler with bilingual support\n";
  llvm::outs() << "Usage: blocktype [options] <source files>\n";
  return 0;
}