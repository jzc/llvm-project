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

template <typename... Ts> auto createStringErrorV(Ts &&...Args) {
  return createStringError(formatv(Args...));
}

Expected<PropertySetRegistry> readPropertiesFromJSON(MemoryBufferRef Buf) {
  PropertySetRegistry Res;
  Expected<json::Value> V = json::parse(Buf.getBuffer());
  if (!V)
    return V.takeError();

  const json::Object *O = V->getAsObject();
  if (!O)
    return createStringErrorV(
        "error while deserializing property set registry: "
        "expected JSON object, got {0}",
        *V);

  for (const auto &[CategoryName, Value] : *O) {
    const json::Object *PropSetVal = Value.getAsObject();
    if (!PropSetVal)
      return createStringErrorV("error while deserializing property set {0}: "
                                "expected JSON array, got {1}",
                                CategoryName.str(), Value);

    PropertySet &PropSet = Res[CategoryName.str()];
    for (const auto &[PropName, PropValueVal] : *PropSetVal) {
      PropertyValue Prop;
      auto PropertyParseError = [&](auto &&...MsgArgs) {
        return createStringErrorV("error while deserializing property {0} "
                                  "in property set {1}: {2}",
                                  PropName.str(), CategoryName.str(),
                                  formatv(MsgArgs...));
      };
      if (std::optional<uint64_t> Val = PropValueVal.getAsUINT64()) {
        Prop = PropertyValue(static_cast<uint32_t>(*Val));
      } else if (const json::Array *Val = PropValueVal.getAsArray()) {
        SmallVector<unsigned char, 0> Vec;
        for (const json::Value &V : *Val) {
          std::optional<uint64_t> Byte = V.getAsUINT64();
          if (!Byte)
            return PropertyParseError("expected a uint64, got {0}", V);
          if (*Byte > std::numeric_limits<unsigned char>::max())
            return PropertyParseError(
                "expected a value between 0 and {0}, got {1}",
                std::numeric_limits<unsigned char>::max(), *Byte);
          Vec.push_back(static_cast<unsigned char>(*Byte));
        }
        Prop = PropertyValue(std::move(Vec));
      } else {
        return PropertyParseError("expected a uint64 or an array, got {0}",
                                  PropValueVal);
      }

      auto [It, Inserted] =
          PropSet.try_emplace(PropName.str(), std::move(Prop));
      assert(Inserted);
    }
  }
  return Res;
}

} // namespace llvm::offloading
