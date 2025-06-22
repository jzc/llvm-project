#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Error.h"

#include <map>
#include <variant>

namespace llvm {
class raw_ostream;
class MemoryBufferRef;

namespace offloading {

using ByteArray = SmallVector<unsigned char, 0>;
using PropertyValue = std::variant<uint32_t, ByteArray>;
using PropertySet = std::map<std::string, PropertyValue>;
using PropertySetRegistry = std::map<std::string, PropertySet>;

void writePropertiesToJSON(const PropertySetRegistry &P, raw_ostream &O);
Expected<PropertySetRegistry> readPropertiesFromJSON(MemoryBufferRef Buf);

} // namespace offloading
} // namespace llvm
