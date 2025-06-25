///===- llvm/Frontend/Offloading/PropertySet.cpp --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Frontend/Offloading/PropertySet.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBufferRef.h"

using namespace llvm;
using namespace llvm::offloading;

void llvm::offloading::writePropertiesToJSON(
    const PropertySetRegistry &PSRegistry, raw_ostream &Out) {
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

Expected<PropertyValue>
readPropertyValueFromJSON(const json::Value &PropValueVal) {
  if (std::optional<uint64_t> Val = PropValueVal.getAsUINT64()) {
    return PropertyValue(static_cast<uint32_t>(*Val));
  }

  if (const json::Array *Val = PropValueVal.getAsArray()) {
    SmallVector<unsigned char, 0> Vec;
    for (const json::Value &V : *Val) {
      std::optional<uint64_t> Byte = V.getAsUINT64();
      if (!Byte)
        return createStringErrorV("expected a uint64, got {0}", V);
      if (*Byte > std::numeric_limits<unsigned char>::max())
        return createStringErrorV("expected a value between 0 and {0}, got {1}",
                                  std::numeric_limits<unsigned char>::max(),
                                  *Byte);
      Vec.push_back(static_cast<unsigned char>(*Byte));
    }
    return PropertyValue(std::move(Vec));
  }

  return createStringErrorV("expected a uint64 or an array, got {0}",
                            PropValueVal);
}

Expected<PropertySetRegistry>
llvm::offloading::readPropertiesFromJSON(MemoryBufferRef Buf) {
  PropertySetRegistry Res;
  Expected<json::Value> V = json::parse(Buf.getBuffer());
  if (auto E = V.takeError())
    return E;

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
      Expected<PropertyValue> Prop = readPropertyValueFromJSON(PropValueVal);
      if (auto E = Prop.takeError())
        return createStringErrorV(
            "error while deserializing property {0} in property set {1}: {2}",
            PropName.str(), CategoryName.str(), toString(std::move(E)));

      auto [It, Inserted] =
          PropSet.try_emplace(PropName.str(), std::move(*Prop));
      assert(Inserted && "Property already exists in PropertySet");
    }
  }
  return Res;
}
