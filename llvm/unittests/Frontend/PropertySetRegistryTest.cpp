#include "llvm/ADT/SmallVector.h"
#include "llvm/Frontend/Offloading/PropertySet.h"
#include "llvm/Support/MemoryBuffer.h"
#include "gtest/gtest.h"

using namespace llvm::offloading;
using namespace llvm;

void checkEquality(const PropertySetRegistry &PSR1,
                   const PropertySetRegistry &PSR2) {
  ASSERT_EQ(PSR1.size(), PSR2.size());
  for (auto It1 = PSR1.begin(), It2 = PSR2.begin(), E = PSR1.end(); It1 != E;
       ++It1, ++It2) {
    const auto &[Category1, PropSet1] = *It1;
    const auto &[Category2, PropSet2] = *It2;
    ASSERT_EQ(Category1, Category2);
    ASSERT_EQ(PropSet1.size(), PropSet2.size());
    for (auto It1 = PropSet1.begin(), It2 = PropSet2.begin(),
              E = PropSet1.end();
         It1 != E; ++It1, ++It2) {
      const auto &[PropName1, PropValue1] = *It1;
      const auto &[PropName2, PropValue2] = *It2;
      ASSERT_EQ(PropName1, PropName2);
      ASSERT_EQ(PropValue1, PropValue2);
    }
  }
}

TEST(PropertySetRegistryTest, PropertySetRegistry) {
  PropertySetRegistry PSR;
  PSR["Category1"]["Prop1"] = 42U;
  PSR["Category1"]["Prop2"] = ByteArray(StringRef("Hello").bytes());
  PSR["Category2"]["A"] = ByteArray{0, 4, 16, 32, 255};
  SmallString<0> Serialized;
  raw_svector_ostream OS(Serialized);
  writePropertiesToJSON(PSR, OS);
  auto PSR2 = readPropertiesFromJSON({Serialized, ""});
  if (auto Err = PSR2.takeError())
    FAIL();
  checkEquality(PSR, *PSR2);
}

TEST(PropertySetRegistryTest, IllFormedJSON) {
  SmallString<0> Serialized;

  // Invalid json
  Serialized = "{ invalid }";
  auto PSR = readPropertiesFromJSON({Serialized, ""});
  ASSERT_TRUE(bool(PSR.takeError()));
  
  Serialized = "";
  PSR = readPropertiesFromJSON({Serialized, ""});
  ASSERT_TRUE(bool(PSR.takeError()));

  // Not a JSON object
  Serialized = "[1, 2, 3]";
  auto PSR = readPropertiesFromJSON({Serialized, ""});
  if (auto Err = PSR.takeError())
    FAIL();
  // ASSERT_TRUE(bool(PSR.takeError()));

  // Property set not an object
  Serialized = R"({ "Category": 42 })";
  PSR = readPropertiesFromJSON({Serialized, ""});
  ASSERT_TRUE(bool(PSR.takeError()));

  // Property value has non array/non-integer type
  Serialized = R"({ "Category": { "Prop": "Value" } })";
  PSR = readPropertiesFromJSON({Serialized, ""});
  ASSERT_TRUE(bool(PSR.takeError()));

  // Property value is an array with non-integer elements
  Serialized = R"({ "Category": { "Prop": [1, "2", 3] } })";
  PSR = readPropertiesFromJSON({Serialized, ""});
  ASSERT_TRUE(bool(PSR.takeError()));

  // Property value is an array with out-of-range integer elements
  Serialized = R"({ "Category": { "Prop": [1, 4294967296, 3] } })";
  PSR = readPropertiesFromJSON({Serialized, ""});
  ASSERT_TRUE(bool(PSR.takeError()));
}
