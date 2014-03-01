#include <apt-pkg/strutl.h>

#include "assert.h"

int main()
{
   std::string input, output, expected;

   // no input
   input = "foobar";
   expected = "foobar";
   output = DeEscapeString(input);
   equals(output, expected);

   // hex and octal
   input = "foo\\040bar\\x0abaz";
   expected = "foo bar\nbaz";
   output = DeEscapeString(input);
   equals(output, expected);

   // at the end
   input = "foo\\040";
   expected = "foo ";
   output = DeEscapeString(input);
   equals(output, expected);

   // double escape
   input = "foo\\\\ x";
   expected = "foo\\ x";
   output = DeEscapeString(input);
   equals(output, expected);

   // double escape at the end
   input = "\\\\foo\\\\";
   expected = "\\foo\\";
   output = DeEscapeString(input);
   equals(output, expected);

   // the string that we actually need it for
   input = "/media/Ubuntu\\04011.04\\040amd64";
   expected = "/media/Ubuntu 11.04 amd64";
   output = DeEscapeString(input);
   equals(output, expected);

   // Split
   input = "status: libnet1:amd64: unpacked";
   vector<std::string> result = StringSplit(input, ": ");
   equals(result[0], "status");
   equals(result[1], "libnet1:amd64");
   equals(result[2], "unpacked");
   equals(result.size(), 3);

   input = "status: libnet1:amd64: unpacked";
   result = StringSplit(input, "xxx");
   equals(result[0], input);
   equals(result.size(), 1);

   input = "status: libnet1:amd64: unpacked";
   result = StringSplit(input, "");
   equals(result.size(), 0);

   input = "x:y:z";
   result = StringSplit(input, ":", 2);
   equals(result.size(), 2);
   equals(result[0], "x");
   equals(result[1], "y:z");

   input = "abc";
   result = StringSplit(input, "");
   equals(result.size(), 0);

   // endswith
   bool b;
   input = "abcd";
   b = APT::String::Endswith(input, "d");
   equals(b, true);

   b = APT::String::Endswith(input, "cd");
   equals(b, true);

   b = APT::String::Endswith(input, "abcd");
   equals(b, true);

   b = APT::String::Endswith(input, "x");
   equals(b, false);

   b = APT::String::Endswith(input, "abcndefg");
   equals(b, false);

   return 0;
}
