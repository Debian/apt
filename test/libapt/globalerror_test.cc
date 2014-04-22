#include <config.h>

#include <apt-pkg/error.h>

#include <stddef.h>
#include <string>
#include <errno.h>
#include <string.h>

#include <gtest/gtest.h>

TEST(GlobalErrorTest,BasicDiscard)
{
   GlobalError e;
   EXPECT_TRUE(e.empty());
   EXPECT_FALSE(e.PendingError());
   EXPECT_FALSE(e.Notice("%s Notice", "A"));
   EXPECT_TRUE(e.empty());
   EXPECT_FALSE(e.empty(GlobalError::DEBUG));
   EXPECT_FALSE(e.PendingError());
   EXPECT_FALSE(e.Error("%s horrible %s %d times", "Something", "happened", 2));
   EXPECT_TRUE(e.PendingError());

   std::string text;
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_TRUE(e.PendingError());
   EXPECT_EQ("A Notice", text);
   EXPECT_TRUE(e.PopMessage(text));
   EXPECT_EQ("Something horrible happened 2 times", text);
   EXPECT_TRUE(e.empty(GlobalError::DEBUG));
   EXPECT_FALSE(e.PendingError());
   EXPECT_FALSE(e.Error("%s horrible %s %d times", "Something", "happened", 2));
   EXPECT_TRUE(e.PendingError());
   EXPECT_FALSE(e.empty(GlobalError::FATAL));
   e.Discard();

   EXPECT_TRUE(e.empty());
   EXPECT_FALSE(e.PendingError());
}
TEST(GlobalErrorTest,StackPushing)
{
   GlobalError e;
   EXPECT_FALSE(e.Notice("%s Notice", "A"));
   EXPECT_FALSE(e.Error("%s horrible %s %d times", "Something", "happened", 2));
   EXPECT_TRUE(e.PendingError());
   EXPECT_FALSE(e.empty(GlobalError::NOTICE));
   e.PushToStack();
   EXPECT_TRUE(e.empty(GlobalError::NOTICE));
   EXPECT_FALSE(e.PendingError());
   EXPECT_FALSE(e.Warning("%s Warning", "A"));
   EXPECT_TRUE(e.empty(GlobalError::ERROR));
   EXPECT_FALSE(e.PendingError());
   e.RevertToStack();
   EXPECT_FALSE(e.empty(GlobalError::ERROR));
   EXPECT_TRUE(e.PendingError());

   std::string text;
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_TRUE(e.PendingError());
   EXPECT_EQ("A Notice", text);
   EXPECT_TRUE(e.PopMessage(text));
   EXPECT_EQ("Something horrible happened 2 times", text);
   EXPECT_FALSE(e.PendingError());
   EXPECT_TRUE(e.empty());

   EXPECT_FALSE(e.Notice("%s Notice", "A"));
   EXPECT_FALSE(e.Error("%s horrible %s %d times", "Something", "happened", 2));
   EXPECT_TRUE(e.PendingError());
   EXPECT_FALSE(e.empty(GlobalError::NOTICE));
   e.PushToStack();
   EXPECT_TRUE(e.empty(GlobalError::NOTICE));
   EXPECT_FALSE(e.PendingError());
   EXPECT_FALSE(e.Warning("%s Warning", "A"));
   EXPECT_TRUE(e.empty(GlobalError::ERROR));
   EXPECT_FALSE(e.PendingError());
   e.MergeWithStack();
   EXPECT_FALSE(e.empty(GlobalError::ERROR));
   EXPECT_TRUE(e.PendingError());
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_TRUE(e.PendingError());
   EXPECT_EQ("A Notice", text);
   EXPECT_TRUE(e.PopMessage(text));
   EXPECT_EQ("Something horrible happened 2 times", text);
   EXPECT_FALSE(e.PendingError());
   EXPECT_FALSE(e.empty());
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_EQ("A Warning", text);
   EXPECT_TRUE(e.empty());
}
TEST(GlobalErrorTest,Errno)
{
   GlobalError e;
   std::string const textOfErrnoZero(strerror(0));
   errno = 0;
   EXPECT_FALSE(e.Errno("errno", "%s horrible %s %d times", "Something", "happened", 2));
   EXPECT_FALSE(e.empty());
   EXPECT_TRUE(e.PendingError());
   std::string text;
   EXPECT_TRUE(e.PopMessage(text));
   EXPECT_FALSE(e.PendingError());
   EXPECT_EQ(std::string("Something horrible happened 2 times - errno (0: ").append(textOfErrnoZero).append(")"), text);
   EXPECT_TRUE(e.empty());
}
TEST(GlobalErrorTest,LongMessage)
{
   GlobalError e;
   std::string const textOfErrnoZero(strerror(0));
   errno = 0;
   std::string text, longText;
   for (size_t i = 0; i < 500; ++i)
      longText.append("a");
   EXPECT_FALSE(e.Error("%s horrible %s %d times", longText.c_str(), "happened", 2));
   EXPECT_TRUE(e.PopMessage(text));
   EXPECT_EQ(std::string(longText).append(" horrible happened 2 times"), text);

   EXPECT_FALSE(e.Errno("errno", "%s horrible %s %d times", longText.c_str(), "happened", 2));
   EXPECT_TRUE(e.PopMessage(text));
   EXPECT_EQ(std::string(longText).append(" horrible happened 2 times - errno (0: ").append(textOfErrnoZero).append(")"), text);
}
TEST(GlobalErrorTest,UTF8Message)
{
   GlobalError e;
   std::string text;

   EXPECT_FALSE(e.Warning("Репозиторий не обновлён и будут %d %s", 4, "test"));
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_EQ("Репозиторий не обновлён и будут 4 test", text);

   std::string longText;
   for (size_t i = 0; i < 50; ++i)
      longText.append("РезийбёбAZ");
   EXPECT_FALSE(e.Warning("%s", longText.c_str()));
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_EQ(longText, text);
}
