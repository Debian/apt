// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Global Error Class - Global error mechanism

   We use a simple STL vector to store each error record. A PendingFlag
   is kept which indicates when the vector contains a Sever error.

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <list>
#include <string>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

									/*}}}*/

// Global Error Object							/*{{{*/
GlobalError *_GetErrorObj()
{
   static thread_local GlobalError Obj;
   return &Obj;
}
									/*}}}*/
// GlobalError::GlobalError - Constructor				/*{{{*/
GlobalError::GlobalError() : PendingFlag(false) {}
									/*}}}*/
// GlobalError::FatalE, Errno, WarningE, NoticeE and DebugE - Add to the list/*{{{*/
#define GEMessage(NAME, TYPE) \
bool GlobalError::NAME (const char *Function, const char *Description,...) { \
	va_list args; \
	size_t msgSize = 400; \
	int const errsv = errno; \
	bool retry; \
	do { \
		va_start(args,Description); \
		retry = InsertErrno(TYPE, Function, Description, args, errsv, msgSize); \
		va_end(args); \
	} while (retry); \
	return false; \
}
GEMessage(FatalE, FATAL)
GEMessage(Errno, ERROR)
GEMessage(WarningE, WARNING)
GEMessage(NoticeE, NOTICE)
GEMessage(DebugE, DEBUG)
#undef GEMessage
									/*}}}*/
// GlobalError::InsertErrno - Get part of the errortype string from errno/*{{{*/
bool GlobalError::InsertErrno(MsgType const &type, const char *Function,
				const char *Description,...) {
	va_list args;
	size_t msgSize = 400;
	int const errsv = errno;
	bool retry;
	do {
		va_start(args,Description);
		retry = InsertErrno(type, Function, Description, args, errsv, msgSize);
		va_end(args);
	} while (retry);
	return false;
}
									/*}}}*/
// GlobalError::InsertErrno - formats an error message with the errno	/*{{{*/
bool GlobalError::InsertErrno(MsgType type, const char* Function,
			      const char* Description, va_list &args,
			      int const errsv, size_t &msgSize) {
	char* S = (char*) malloc(msgSize);
	int const n = snprintf(S, msgSize, "%s - %s (%i: %s)", Description,
			       Function, errsv, strerror(errsv));
	if (n > -1 && ((unsigned int) n) < msgSize);
	else {
		if (n > -1)
			msgSize = n + 1;
		else
			msgSize *= 2;
		free(S);
		return true;
	}

	bool const geins = Insert(type, S, args, msgSize);
	free(S);
	return geins;
}
									/*}}}*/
// GlobalError::Fatal, Error, Warning, Notice and Debug - Add to the list/*{{{*/
#define GEMessage(NAME, TYPE) \
bool GlobalError::NAME (const char *Description,...) { \
	va_list args; \
	size_t msgSize = 400; \
	bool retry; \
	do { \
		va_start(args,Description); \
		retry = Insert(TYPE, Description, args, msgSize); \
		va_end(args); \
	} while (retry); \
	return false; \
}
GEMessage(Fatal, FATAL)
GEMessage(Error, ERROR)
GEMessage(Warning, WARNING)
GEMessage(Notice, NOTICE)
GEMessage(Debug, DEBUG)
#undef GEMessage
									/*}}}*/
// GlobalError::Insert - Add a errotype message to the list		/*{{{*/
bool GlobalError::Insert(MsgType const &type, const char *Description,...)
{
	va_list args;
	size_t msgSize = 400;
	bool retry;
	do {
		va_start(args,Description);
		retry = Insert(type, Description, args, msgSize);
		va_end(args);
	} while (retry);
	return false;
}
									/*}}}*/
// GlobalError::Insert - Insert a new item at the end			/*{{{*/
bool GlobalError::Insert(MsgType type, const char* Description,
			 va_list &args, size_t &msgSize) {
	char* S = (char*) malloc(msgSize);
	int const n = vsnprintf(S, msgSize, Description, args);
	if (n > -1 && ((unsigned int) n) < msgSize);
	else {
		if (n > -1)
			msgSize = n + 1;
		else
			msgSize *= 2;
 		free(S);
		return true;
	}

	Item const m(S, type);
	Messages.push_back(m);

	if (type == ERROR || type == FATAL)
		PendingFlag = true;

	if (type == FATAL || type == DEBUG)
		std::clog << m << std::endl;

	free(S);
	return false;
}
									/*}}}*/
// GlobalError::PopMessage - Pulls a single message out			/*{{{*/
bool GlobalError::PopMessage(std::string &Text) {
	if (Messages.empty() == true)
		return false;

	Item const msg = Messages.front();
	Messages.pop_front();

	bool const Ret = (msg.Type == ERROR || msg.Type == FATAL);
	Text = msg.Text;
	if (PendingFlag == false || Ret == false)
		return Ret;

	// check if another error message is pending
	for (std::list<Item>::const_iterator m = Messages.begin();
	     m != Messages.end(); ++m)
		if (m->Type == ERROR || m->Type == FATAL)
			return Ret;

	PendingFlag = false;
	return Ret;
}
									/*}}}*/
// GlobalError::DumpErrors - Dump all of the errors/warns to cerr	/*{{{*/
void GlobalError::DumpErrors(std::ostream &out, MsgType const &threshold,
			     bool const &mergeStack) {
	if (mergeStack == true)
		for (std::list<MsgStack>::const_reverse_iterator s = Stacks.rbegin();
		     s != Stacks.rend(); ++s)
			std::copy(s->Messages.begin(), s->Messages.end(), std::front_inserter(Messages));

	std::for_each(Messages.begin(), Messages.end(), [&threshold, &out](Item const &m) {
		if (m.Type >= threshold)
			out << m << std::endl;
	});

	Discard();
}
									/*}}}*/
// GlobalError::Discard - Discard					/*{{{*/
void GlobalError::Discard() {
	Messages.clear();
	PendingFlag = false;
}
									/*}}}*/
// GlobalError::empty - does our error list include anything?		/*{{{*/
bool GlobalError::empty(MsgType const &threshold) const {
	if (PendingFlag == true)
		return false;

	if (Messages.empty() == true)
		return true;

	return std::find_if(Messages.begin(), Messages.end(), [&threshold](Item const &m) {
		return m.Type >= threshold;
	}) == Messages.end();
}
									/*}}}*/
// GlobalError::PushToStack						/*{{{*/
void GlobalError::PushToStack() {
	Stacks.emplace_back(Messages, PendingFlag);
	Discard();
}
									/*}}}*/
// GlobalError::RevertToStack						/*{{{*/
void GlobalError::RevertToStack() {
	Discard();
	MsgStack pack = Stacks.back();
	Messages = pack.Messages;
	PendingFlag = pack.PendingFlag;
	Stacks.pop_back();
}
									/*}}}*/
// GlobalError::MergeWithStack						/*{{{*/
void GlobalError::MergeWithStack() {
	MsgStack pack = Stacks.back();
	Messages.splice(Messages.begin(), pack.Messages);
	PendingFlag = PendingFlag || pack.PendingFlag;
	Stacks.pop_back();
}
									/*}}}*/

// GlobalError::Item::operator<<						/*{{{*/
APT_HIDDEN std::ostream &operator<<(std::ostream &out, GlobalError::Item i)
{
   static constexpr auto COLOR_RESET = "\033[0m";
   static constexpr auto COLOR_NOTICE = "\033[33m";  // normal yellow
   static constexpr auto COLOR_WARN = "\033[1;33m";  // bold yellow
   static constexpr auto COLOR_ERROR = "\033[1;31m"; // bold red

   bool use_color = _config->FindB("APT::Color", false);

   if (use_color)
   {
      switch (i.Type)
      {
      case GlobalError::FATAL:
      case GlobalError::ERROR:
	 out << COLOR_ERROR;
	 break;
      case GlobalError::WARNING:
	 out << COLOR_WARN;
	 break;
      case GlobalError::NOTICE:
	 out << COLOR_NOTICE;
	 break;
      default:
	 break;
      }
   }

   switch (i.Type)
   {
   case GlobalError::FATAL:
   case GlobalError::ERROR:
      out << 'E';
      break;
   case GlobalError::WARNING:
      out << 'W';
      break;
   case GlobalError::NOTICE:
      out << 'N';
      break;
   case GlobalError::DEBUG:
      out << 'D';
      break;
   }
   out << ": ";

   if (use_color)
   {
      switch (i.Type)
      {
      case GlobalError::FATAL:
      case GlobalError::ERROR:
      case GlobalError::WARNING:
      case GlobalError::NOTICE:
	 out << COLOR_RESET;
	 break;
      default:
	 break;
      }
   }

   std::string::size_type line_start = 0;
   std::string::size_type line_end;
   while ((line_end = i.Text.find_first_of("\n\r", line_start)) != std::string::npos)
   {
      if (line_start != 0)
	 out << std::endl
	     << "   ";
      out << i.Text.substr(line_start, line_end - line_start);
      line_start = i.Text.find_first_not_of("\n\r", line_end + 1);
      if (line_start == std::string::npos)
	 break;
   }
   if (line_start == 0)
      out << i.Text;
   else if (line_start != std::string::npos)
      out << std::endl
	  << "   " << i.Text.substr(line_start);

   if (use_color)
      out << COLOR_RESET;

   return out;
}
									/*}}}*/
