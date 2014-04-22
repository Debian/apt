#include <gtest/gtest.h>
#include <apt-pkg/error.h>
int main(int argc, char **argv) {
   ::testing::InitGoogleTest(&argc, argv);
   int result = RUN_ALL_TESTS();
   if (_error->empty() == false)
   {
      std::cerr << "The test generated the following global messages:" << std::endl;
      _error->DumpErrors(std::cerr);
      // messages on the stack can't be right, error out
      // even if we have no idea where this message came from
      if (result == 0)
      {
	 std::cerr << "All tests successful, but messages were generated, so still a failure!" << std::endl;
	 return 29;
      }
   }
   return result;
}
