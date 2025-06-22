#include "llvm/Frontend/Offloading/PropertySet.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBufferRef.h"

namespace llvm::offloading {

void writePropertiesToJSON(const PropertySetRegistry &PSRegistry,
                           raw_ostream &Out) {
  json::OStream J(Out);
  J.object([&] {
    for (const auto &[CategoryName, PropSet] : PSRegistry) {
      J.attributeObject(CategoryName, [&] {
        for (const auto &[PropName, PropVal] : PropSet) {
          switch (PropVal.index()) {
          case 0:
            J.attribute(PropName, std::get<uint32_t>(PropVal));
            break;
          case 1:
            J.attributeArray(PropName, [&] {
              for (const auto &Byte : std::get<ByteArray>(PropVal)) {
                J.value(Byte);
              }
            });
            break;
          default:
            llvm_unreachable("unsupported property type");
          }
        }
      });
    }
  });
}

Expected<PropertySetRegistry> readPropertiesFromJSON(MemoryBufferRef Buf) {
  PropertySetRegistry Res;
  Expected<json::Value> V = json::parse(Buf.getBuffer());
  if (!V)
    return V.takeError();

  const json::Object *O = V->getAsObject();
  if (!O)
    return createStringError("expected JSON object");

  for (const auto &[CategoryName, Value] : *O) {
    const json::Object *PropSetVal = Value.getAsObject();
    if (!PropSetVal)
      return createStringError("expected JSON array for properties");

    PropertySet &PropSet = Res[CategoryName.str()];
    for (const auto &[PropName, PropValueVal] : *PropSetVal) {
      PropertyValue Prop;
      if (std::optional<uint64_t> Val = PropValueVal.getAsUINT64()) {
        Prop = PropertyValue(static_cast<uint32_t>(*Val));
      } else if (const json::Array *Val = PropValueVal.getAsArray()) {
        SmallVector<unsigned char, 0> Vec;
        for (const json::Value &V : *Val) {
          std::optional<uint64_t> Byte = V.getAsUINT64();
          if (!Byte)
            return createStringError("invalid byte array value");
          if (*Byte > std::numeric_limits<unsigned char>::max())
            return createStringError("byte array value out of range");
          Vec.push_back(static_cast<unsigned char>(*Byte));
        }
        Prop = PropertyValue(std::move(Vec));
      } else {
        return createStringError("unsupported property type");
      }

      auto [It, Inserted] =
          PropSet.try_emplace(PropName.str(), std::move(Prop));
      if (!Inserted)
        return createStringError("duplicate property name");
    }
  }
  return Res;
}

} // namespace llvm::offloading
