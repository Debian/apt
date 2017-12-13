#include <config.h>

#include <apt-pkg/error.h>

#include <string>
#include <errno.h>
#include <stddef.h>
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
   EXPECT_EQ(longText + " horrible happened 2 times", text);

   EXPECT_FALSE(e.Errno("errno", "%s horrible %s %d times", longText.c_str(), "happened", 2));
   EXPECT_TRUE(e.PopMessage(text));
   EXPECT_EQ(longText + " horrible happened 2 times - errno (0: " + textOfErrnoZero + ")", text);

   EXPECT_FALSE(e.Error("%s horrible %s %d times", longText.c_str(), "happened", 2));
   std::ostringstream out;
   e.DumpErrors(out);
   EXPECT_EQ(std::string("E: ").append(longText).append(" horrible happened 2 times\n"), out.str());

   EXPECT_FALSE(e.Errno("errno", "%s horrible %s %d times", longText.c_str(), "happened", 2));
   std::ostringstream out2;
   e.DumpErrors(out2);
   EXPECT_EQ(std::string("E: ").append(longText).append(" horrible happened 2 times - errno (0: ").append(textOfErrnoZero).append(")\n"), out2.str());
}
TEST(GlobalErrorTest,UTF8Message)
{
   GlobalError e;
   std::string text;

   EXPECT_FALSE(e.Warning("Репозиторий не обновлён и будут %d %s", 4, "test"));
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_EQ("Репозиторий не обновлён и будут 4 test", text);

   EXPECT_FALSE(e.Warning("Репозиторий не обновлён и будут %d %s", 4, "test"));
   std::ostringstream out;
   e.DumpErrors(out);
   EXPECT_EQ("W: Репозиторий не обновлён и будут 4 test\n", out.str());

   std::string longText;
   for (size_t i = 0; i < 50; ++i)
      longText.append("РезийбёбAZ");
   EXPECT_FALSE(e.Warning("%s", longText.c_str()));
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_EQ(longText, text);
}
TEST(GlobalErrorTest,MultiLineMessage)
{
   GlobalError e;
   std::string text;

   EXPECT_FALSE(e.Warning("Sometimes one line isn't enough.\nYou do know what I mean, right?\r\n%s?\rGood because I don't.", "Right"));
   EXPECT_FALSE(e.PopMessage(text));
   EXPECT_EQ("Sometimes one line isn't enough.\nYou do know what I mean, right?\r\nRight?\rGood because I don't.", text);

   EXPECT_FALSE(e.Warning("Sometimes one line isn't enough.\nYou do know what I mean, right?\r\n%s?\rGood because I don't.", "Right"));
   std::ostringstream out;
   e.DumpErrors(out);
   EXPECT_EQ("W: Sometimes one line isn't enough.\n   You do know what I mean, right?\n   Right?\n   Good because I don't.\n", out.str());

   EXPECT_FALSE(e.Warning("Sometimes one line isn't enough.\nYou do know what I mean, right?\r\n%s?\rGood because I don't.\n", "Right"));
   std::ostringstream out2;
   e.DumpErrors(out2);
   EXPECT_EQ("W: Sometimes one line isn't enough.\n   You do know what I mean, right?\n   Right?\n   Good because I don't.\n", out2.str());
}
