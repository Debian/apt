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
// GlobalError::FatalE - Get part of the error string from errno	/*{{{*/
bool GlobalError::FatalE(const char *Function,const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return InsertErrno(FATAL, Function, Description, args);
}
									/*}}}*/
// GlobalError::Errno - Get part of the error string from errno		/*{{{*/
bool GlobalError::Errno(const char *Function,const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return InsertErrno(ERROR, Function, Description, args);
}
									/*}}}*/
// GlobalError::WarningE - Get part of the warning string from errno	/*{{{*/
bool GlobalError::WarningE(const char *Function,const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return InsertErrno(WARNING, Function, Description, args);
}
									/*}}}*/
// GlobalError::NoticeE - Get part of the notice string from errno	/*{{{*/
bool GlobalError::NoticeE(const char *Function,const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return InsertErrno(NOTICE, Function, Description, args);
}
									/*}}}*/
// GlobalError::DebugE - Get part of the debug string from errno	/*{{{*/
bool GlobalError::DebugE(const char *Function,const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return InsertErrno(DEBUG, Function, Description, args);
}
									/*}}}*/
// GlobalError::InsertErrno - Get part of the errortype string from errno/*{{{*/
bool GlobalError::InsertErrno(MsgType const &type, const char *Function,
				const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return InsertErrno(type, Function, Description, args);
}
									/*}}}*/
// GlobalError::InsertErrno - formats an error message with the errno	/*{{{*/
bool GlobalError::InsertErrno(MsgType type, const char* Function,
			      const char* Description, va_list &args) {
	char S[400];
	snprintf(S, sizeof(S), "%s - %s (%i: %s)", Description,
		 Function, errno, strerror(errno));
	return Insert(type, S, args);
}
									/*}}}*/
// GlobalError::Fatal - Add a fatal error to the list			/*{{{*/
bool GlobalError::Fatal(const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return Insert(FATAL, Description, args);
}
									/*}}}*/
// GlobalError::Error - Add an error to the list			/*{{{*/
bool GlobalError::Error(const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return Insert(ERROR, Description, args);
}
									/*}}}*/
// GlobalError::Warning - Add a warning to the list			/*{{{*/
bool GlobalError::Warning(const char *Description,...) {
	va_list args;
	va_start(args,Description);
	return Insert(WARNING, Description, args);
}
									/*}}}*/
// GlobalError::Notice - Add a notice to the list			/*{{{*/
bool GlobalError::Notice(const char *Description,...)
{
	va_list args;
	va_start(args,Description);
	return Insert(NOTICE, Description, args);
}
									/*}}}*/
// GlobalError::Debug - Add a debug to the list				/*{{{*/
bool GlobalError::Debug(const char *Description,...)
{
	va_list args;
	va_start(args,Description);
	return Insert(DEBUG, Description, args);
}
									/*}}}*/
// GlobalError::Insert - Add a errotype message to the list		/*{{{*/
bool GlobalError::Insert(MsgType const &type, const char *Description,...)
{
	va_list args;
	va_start(args,Description);
	return Insert(type, Description, args);
}
									/*}}}*/
// GlobalError::Insert - Insert a new item at the end			/*{{{*/
bool GlobalError::Insert(MsgType type, const char* Description,
			 va_list &args) {
	char S[400];
	vsnprintf(S,sizeof(S),Description,args);

	Item const m(S, type);
	Messages.push_back(m);

	if (type == ERROR || type == FATAL)
		PendingFlag = true;

	if (type == FATAL || type == DEBUG)
		std::clog << m << std::endl;

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
