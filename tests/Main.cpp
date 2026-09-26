#include "TestHelper.h"
#include <gtest/gtest.h>

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new JamokeTestEnvironment());
  return RUN_ALL_TESTS();
}
