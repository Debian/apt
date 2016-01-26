#include <gtest/gtest.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>

int main(int argc, char **argv) {
   ::testing::InitGoogleTest(&argc, argv);
   if (pkgInitSystem(*_config, _system) == false)
      return 42;
   int const result = RUN_ALL_TESTS();
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
