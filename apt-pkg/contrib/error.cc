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
#include <apt-pkg/error.h>

#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <cstring>

#include "config.h"
   									/*}}}*/

// Global Error Object							/*{{{*/
/* If the implementation supports posix threads then the accessor function
   is compiled to be thread safe otherwise a non-safe version is used. A
   Per-Thread error object is maintained in much the same manner as libc
   manages errno */
#if defined(_POSIX_THREADS) && defined(HAVE_PTHREAD)
	#include <pthread.h>

	static pthread_key_t ErrorKey;
	static void ErrorDestroy(void *Obj) {delete (GlobalError *)Obj;};
	static void KeyAlloc() {pthread_key_create(&ErrorKey,ErrorDestroy);};

	GlobalError *_GetErrorObj() {
		static pthread_once_t Once = PTHREAD_ONCE_INIT;
		pthread_once(&Once,KeyAlloc);

		void *Res = pthread_getspecific(ErrorKey);
		if (Res == 0)
			pthread_setspecific(ErrorKey,Res = new GlobalError);
		return (GlobalError *)Res;
	}
#else
	GlobalError *_GetErrorObj() {
		static GlobalError *Obj = new GlobalError;
		return Obj;
	}
#endif
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
	while (true) { \
		va_start(args,Description); \
		if (InsertErrno(TYPE, Function, Description, args, errsv, msgSize) == false) \
			break; \
		va_end(args); \
	} \
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
	while (true) {
		va_start(args,Description);
		if (InsertErrno(type, Function, Description, args, errsv, msgSize) == false)
			break;
		va_end(args);
	}
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
	while (true) { \
		va_start(args,Description); \
		if (Insert(TYPE, Description, args, msgSize) == false) \
			break; \
		va_end(args); \
	} \
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
	while (true) {
		va_start(args,Description);
		if (Insert(type, Description, args, msgSize) == false)
			break;
		va_end(args);
	}
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
	     m != Messages.end(); m++)
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
			Messages.insert(Messages.begin(), s->Messages.begin(), s->Messages.end());

	for (std::list<Item>::const_iterator m = Messages.begin();
	     m != Messages.end(); m++)
		if (m->Type >= threshold)
			out << (*m) << std::endl;
	Discard();
}
									/*}}}*/
// GlobalError::Discard - Discard					/*{{{*/
void GlobalError::Discard() {
	Messages.clear();
	PendingFlag = false;
};
									/*}}}*/
// GlobalError::empty - does our error list include anything?		/*{{{*/
bool GlobalError::empty(MsgType const &trashhold) const {
	if (PendingFlag == true)
		return false;

	if (Messages.empty() == true)
		return true;

	for (std::list<Item>::const_iterator m = Messages.begin();
	     m != Messages.end(); m++)
		if (m->Type >= trashhold)
			return false;

	return true;
}
									/*}}}*/
// GlobalError::PushToStack						/*{{{*/
void GlobalError::PushToStack() {
	MsgStack pack(Messages, PendingFlag);
	Stacks.push_back(pack);
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
	Messages.insert(Messages.begin(), pack.Messages.begin(), pack.Messages.end());
	PendingFlag = PendingFlag || pack.PendingFlag;
	Stacks.pop_back();
}
									/*}}}*/
