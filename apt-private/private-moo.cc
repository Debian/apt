// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Here be cows – but: Never ask, never tell

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-moo.h>
#include <apt-private/private-output.h>
#include <apt-private/private-utils.h>

#include <stddef.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <string>

#include <apti18n.h>
									/*}}}*/

static std::string getMooLine(time_t const timenow)			/*{{{*/
{
   struct tm special;
   localtime_r(&timenow, &special);
   enum { NORMAL, PACKAGEMANAGER, APPRECIATION, AGITATION, AIRBORN } line;
   if (special.tm_mon == 11 && special.tm_mday == 25)
      line = PACKAGEMANAGER;
   else if (special.tm_mon == 7 && special.tm_mday == 16)
      line = APPRECIATION;
   else if (special.tm_mon == 10 && special.tm_mday == 7)
      line = AGITATION;
   else if (special.tm_mon == 1 && special.tm_mday == 18)
      line = AIRBORN;
   else
      line = NORMAL;

   bool const quiet = _config->FindI("quiet") >= 2;
   std::ostringstream out;
   if (quiet == false)
      out << "...\"";

   switch(line)
   {
      case PACKAGEMANAGER: out << "Happy package management day!"; break;
      case APPRECIATION:   out << "Three moos for Debian!"; break;
      case AGITATION:      out << "Whoever needs milk, bows to the animal."; break;
      case AIRBORN:        out << "It's a Bird ... It's a Plane ... It's Super Cow!"; break;
      default:             out << "Have you mooed today?"; break;
   }

   if (quiet == true)
      out << std::endl;
   else
      out << "\"..." << std::endl;

   return out.str();
}
									/*}}}*/
static bool printMooLine(time_t const timenow)				/*{{{*/
{
   std::cerr << getMooLine(timenow);
   return true;
}
									/*}}}*/
static bool DoMoo1(time_t const timenow)				/*{{{*/
{
   // our trustworthy super cow since 2001
   if (_config->FindI("quiet") >= 2)
      return printMooLine(timenow);
   std::string const moo = getMooLine(timenow);
   size_t const depth = moo.length()/4;
   c1out <<
      OutputInDepth(depth, " ") << "         (__) \n" <<
      OutputInDepth(depth, " ") << "         (oo) \n" <<
      OutputInDepth(depth, " ") << "   /------\\/ \n" <<
      OutputInDepth(depth, " ") << "  / |    ||   \n" <<
      OutputInDepth(depth, " ") << " *  /\\---/\\ \n" <<
      OutputInDepth(depth, " ") << "    ~~   ~~   \n" <<
      moo;
   return true;
}
									/*}}}*/
static bool DoMoo2(time_t const timenow)				/*{{{*/
{
   // by Fernando Ribeiro in lp:56125
   if (_config->FindI("quiet") >= 2)
      return printMooLine(timenow);
   std::string const moo = getMooLine(timenow);
   size_t const depth = moo.length()/4;
   if (_config->FindB("APT::Moo::Color", false) == false)
      c1out <<
	 OutputInDepth(depth, " ") << "         (__)  \n" <<
	 OutputInDepth(depth, " ") << " _______~(..)~ \n" <<
	 OutputInDepth(depth, " ") << "   ,----\\(oo) \n" <<
	 OutputInDepth(depth, " ") << "  /|____|,'    \n" <<
	 OutputInDepth(depth, " ") << " * /\"\\ /\\   \n" <<
	 OutputInDepth(depth, " ") << "   ~ ~ ~ ~     \n" <<
	 moo;
   else
   {
      c1out <<
	 OutputInDepth(depth, " ") << "         \033[1;97m(\033[0;33m__\033[1;97m)\033[0m\n" <<
	 OutputInDepth(depth, " ") << " \033[31m_______\033[33m~(\033[1;34m..\033[0;33m)~\033[0m\n" <<
	 OutputInDepth(depth, " ") << "   \033[33m,----\033[31m\\\033[33m(\033[1;4;35moo\033[0;33m)\033[0m\n" <<
	 OutputInDepth(depth, " ") << "  \033[33m/|____|,'\033[0m\n" <<
	 OutputInDepth(depth, " ") << " \033[1;5;97m*\033[0;33m /\\  /\\\033[0m\n" <<
	 "\033[32m";
      for (size_t i = moo.length()/2; i > 1; --i)
	 c1out << "wW";

      c1out << "w\033[0m\n" << moo;
   }

   return true;
}
									/*}}}*/
static bool DoMoo3(time_t const timenow)				/*{{{*/
{
   // by Robert Millan in deb:134156
   if (_config->FindI("quiet") >= 2)
      return printMooLine(timenow);
   std::string const moo = getMooLine(timenow);
   size_t const depth = moo.length()/16;
   c1out <<
      OutputInDepth(depth, " ") << "                   \\_/ \n" <<
      OutputInDepth(depth, " ") << " m00h  (__)       -(_)- \n" <<
      OutputInDepth(depth, " ") << "    \\  ~Oo~___     / \\\n" <<
      OutputInDepth(depth, " ") << "       (..)  |\\        \n" <<
      OutputInDepth(depth, "_") << "_________|_|_|__________" <<
      OutputInDepth((moo.length() - (depth + 27)), "_") << "\n" << moo;
   return true;
}
									/*}}}*/
static bool DoMooApril()						/*{{{*/
{
   // by Christopher Allan Webber and proposed by Paul Tagliamonte
   // in a "Community outreach": https://lists.debian.org/debian-devel/2013/04/msg00045.html
   if (_config->FindI("quiet") >= 2)
   {
      std::cerr << "Have you smashed some milk today?" << std::endl;
      return true;
   }
   c1out <<
      "               _     _\n"
      "              (_\\___( \\,\n"
      "                )___   _  Have you smashed some milk today?\n"
      "               /( (_)-(_)    /\n"
      "    ,---------'         \\_\n"
      "  //(  ',__,'      \\  (' ')\n"
      " //  )              '----'\n"
      " '' ; \\     .--.  ,/\n"
      "    | )',_,'----( ;\n"
      "    ||| '''     '||\n";
   return true;
}
									/*}}}*/
bool DoMoo(CommandLine &CmdL)						/*{{{*/
{
   time_t const timenow = GetSecondsSinceEpoch();

   struct tm april;
   localtime_r(&timenow, &april);
   if (april.tm_mday == 1 && april.tm_mon == 3)
      return DoMooApril();

   signed short SuperCow = 1;
   if (CmdL.FileSize() != 0)
      for (const char **Moo = CmdL.FileList + 1; *Moo != 0; Moo++)
         if (strcasecmp(*Moo, "moo") == 0)
            SuperCow++;

   // time is random enough for our purpose
   if (SuperCow > 3)
   {
      if (april.tm_sec == 1)
	 SuperCow = 1 + (timenow % 4);
      else
	 SuperCow = 1 + (timenow % 3);
   }

   switch(SuperCow) {
      case 1: return DoMoo1(timenow);
      case 2: return DoMoo2(timenow);
      case 3: return DoMoo3(timenow);
      case 4: return DoMooApril();
      default: return DoMoo1(timenow);
   }

   return true;
}
									/*}}}*/
