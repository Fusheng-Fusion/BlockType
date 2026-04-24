#include "blocktype/IR/IRFormat.h"

#include <sstream>

namespace blocktype {
namespace ir {

std::string IRFormatVersion::toString() const {
  std::ostringstream OS;
  OS << static_cast<unsigned>(Major) << "."
     << static_cast<unsigned>(Minor) << "."
     << static_cast<unsigned>(Patch);
  return OS.str();
}

} // namespace ir
} // namespace blocktype
