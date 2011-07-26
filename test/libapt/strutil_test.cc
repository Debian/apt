#include <apt-pkg/strutl.h>

#include "assert.h"

int main(int argc,char *argv[])
{
   string input, output, expected;

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

   return 0;
}
