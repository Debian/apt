// -*- mode: cpp; mode: fold -*-
// SPDX-License-Identifier: GPL-2.0+
// Description								/*{{{*/
/* ######################################################################
   
   Global Error Class - Global error mechanism

   This class has a single global instance. When a function needs to 
   generate an error condition, such as a read error, it calls a member
   in this class to add the error to a stack of errors. 
   
   By using a stack the problem with a scheme like errno is removed and
   it allows a very detailed account of what went wrong to be transmitted
   to the UI for display. (Errno has problems because each function sets
   errno to 0 if it didn't have an error thus eraseing erno in the process
   of cleanup)
   
   Several predefined error generators are provided to handle common 
   things like errno. The general idea is that all methods return a bool.
   If the bool is true then things are OK, if it is false then things 
   should start being undone and the stack should unwind under program
   control.
   
   A Warning should not force the return of false. Things did not fail, but
   they might have had unexpected problems. Errors are stored in a FIFO
   so Pop will return the first item..
   
   I have some thoughts about extending this into a more general UI<-> 
   Engine interface, ie allowing the Engine to say 'The disk is full' in 
   a dialog that says 'Panic' and 'Retry'.. The error generator functions
   like errno, Warning and Error return false always so this is normal:
     if (open(..))
        return _error->Errno(..);
   
   This file had this historic note, but now includes further changes
   under the GPL-2.0+:

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ERROR_H
#define PKGLIB_ERROR_H

#include <apt-pkg/macros.h>

#include <iostream>
#include <list>
#include <string>

#include <cstdarg>
#include <cstddef>

class APT_PUBLIC GlobalError						/*{{{*/
{
public:									/*{{{*/
	/** \brief a message can have one of following severity */
	enum MsgType {
		/** \brief Message will be printed instantly as it is likely that
			this error will lead to a complete crash */
		FATAL = 40,
		/** \brief An error does hinder the correct execution and should be corrected */
		ERROR = 30,
		/** \brief indicates problem that can lead to errors later on */
		WARNING = 20,
		/** \brief deprecation warnings, old fallback behavior, … */
		NOTICE = 10,
		/** \brief for developers only in areas it is hard to print something directly */
		DEBUG = 0
	};

	/** \brief add a fatal error message with errno to the list
	 *
	 *  \param Function name of the function generating the error
	 *  \param Description format string for the error message
	 *
	 *  \return \b false
	 */
	bool FatalE(const char *Function,const char *Description,...) APT_PRINTF(3) APT_COLD;

	/** \brief add an Error message with errno to the list
	 *
	 *  \param Function name of the function generating the error
	 *  \param Description format string for the error message
	 *
	 *  \return \b false
	 */
	bool Errno(const char *Function,const char *Description,...) APT_PRINTF(3) APT_COLD;

	/** \brief add a warning message with errno to the list
	 *
	 *  A warning should be considered less severe than an error and
	 *  may be ignored by the client.
	 *
	 *  \param Function Name of the function generates the warning.
	 *  \param Description Format string for the warning message.
	 *
	 *  \return \b false
	 */
	bool WarningE(const char *Function,const char *Description,...) APT_PRINTF(3) APT_COLD;

	/** \brief add a notice message with errno to the list
	 *
	 *  \param Function name of the function generating the error
	 *  \param Description format string for the error message
	 *
	 *  \return \b false
	 */
	bool NoticeE(const char *Function,const char *Description,...) APT_PRINTF(3) APT_COLD;

	/** \brief add a debug message with errno to the list
	 *
	 *  \param Function name of the function generating the error
	 *  \param Description format string for the error message
	 *
	 *  \return \b false
	 */
	bool DebugE(const char *Function,const char *Description,...) APT_PRINTF(3) APT_COLD;

	/** \brief adds an errno message with the given type
	 *
	 * \param type of the error message
	 * \param Function which failed
	 * \param Description of the error
	 */
	bool InsertErrno(MsgType const &type, const char* Function,
			 const char* Description,...) APT_PRINTF(4) APT_COLD;

	/** \brief adds an errno message with the given type
	 *
	 * args needs to be initialized with va_start and terminated
	 * with va_end by the caller. msgSize is also an out-parameter
	 * in case the msgSize was not enough to store the complete message.
	 *
	 * \param type of the error message
	 * \param Function which failed
	 * \param Description is the format string for args
	 * \param args list from a printf-like function
	 * \param errsv is the errno the error is for
	 * \param msgSize is the size of the char[] used to store message
	 * \return true if the message was added, false if not - the caller
	 * should call this method again in that case
	 */
	bool InsertErrno(MsgType type, const char* Function,
			 const char* Description, va_list &args,
			 int const errsv, size_t &msgSize) APT_COLD;

	/** \brief add an fatal error message to the list
	 *
	 *  Most of the stuff we consider as "error" is also "fatal" for
	 *  the user as the application will not have the expected result,
	 *  but a fatal message here means that it gets printed directly
	 *  to stderr in addition to adding it to the list as the error
	 *  leads sometimes to crashes and a maybe duplicated message
	 *  is better than "Segfault" as the only displayed text
	 *
	 *  \param Description Format string for the fatal error message.
	 *
	 *  \return \b false
	 */
	bool Fatal(const char *Description,...) APT_PRINTF(2) APT_COLD;

	/** \brief add an Error message to the list
	 *
	 *  \param Description Format string for the error message.
	 *
	 *  \return \b false
	 */
	bool Error(const char *Description,...) APT_PRINTF(2) APT_COLD;

	/** \brief add a warning message to the list
	 *
	 *  A warning should be considered less severe than an error and
	 *  may be ignored by the client.
	 *
	 *  \param Description Format string for the message
	 *
	 *  \return \b false
	 */
	bool Warning(const char *Description,...) APT_PRINTF(2) APT_COLD;

	/** \brief add a notice message to the list
	 *
	 *  A notice should be considered less severe than an error or a
	 *  warning and can be ignored by the client without further problems
	 *  for some times, but he should consider fixing the problem.
	 *  This error type can be used for e.g. deprecation warnings of options.
	 *
	 *  \param Description Format string for the message
	 *
	 *  \return \b false
	 */
	bool Notice(const char *Description,...) APT_PRINTF(2) APT_COLD;

	/** \brief add a debug message to the list
	 *
	 *  \param Description Format string for the message
	 *
	 *  \return \b false
	 */
	bool Debug(const char *Description,...) APT_PRINTF(2) APT_COLD;

	/** \brief adds an error message with the given type
	 *
	 * \param type of the error message
	 * \param Description of the error
	 */
	bool Insert(MsgType const &type, const char* Description,...) APT_PRINTF(3) APT_COLD;

	/** \brief adds an error message with the given type
	 *
	 * args needs to be initialized with va_start and terminated
	 * with va_end by the caller. msgSize is also an out-parameter
	 * in case the msgSize was not enough to store the complete message.
	 *
	 * \param type of the error message
	 * \param Description is the format string for args
	 * \param args list from a printf-like function
	 * \param msgSize is the size of the char[] used to store message
	 * \return true if the message was added, false if not - the caller
	 * should call this method again in that case
	 */
	bool Insert(MsgType type, const char* Description,
			 va_list &args, size_t &msgSize) APT_COLD;

	/** \brief is an error in the list?
	 *
	 *  \return \b true if an error is included in the list, \b false otherwise
	 */
	inline bool PendingError() const APT_PURE {return PendingFlag;};

	/** \brief is the list empty?
	 *
	 *  Can be used to check if the current stack level doesn't include
	 *  anything equal or more severe than a given threshold, defaulting
	 *  to warning level for historic reasons.
	 *
	 *  \param threshold minimum level considered
	 *
	 *  \return \b true if the list is empty, \b false otherwise
	 */
	bool empty(MsgType const &threshold = WARNING) const APT_PURE;

	/** \brief returns and removes the first (or last) message in the list
	 *
	 *  \param[out] Text message of the first/last item
	 *
	 *  \return \b true if the message was an error, \b false otherwise
	 */
	bool PopMessage(std::string &Text);

	/** \brief clears the list of messages */
	void Discard();

	/** \brief outputs the list of messages to the given stream
	 *
	 *  Note that all messages are discarded, even undisplayed ones.
	 *
	 *  \param[out] out output stream to write the messages in
	 *  \param threshold minimum level considered
	 *  \param mergeStack if true recursively dumps the entire stack
	 */
	void DumpErrors(std::ostream &out, MsgType const &threshold = WARNING,
			bool const &mergeStack = true);

	/** \brief dumps the list of messages to std::cerr
	 *
	 *  Note that all messages are discarded, also the notices
	 *  displayed or not.
	 *
	 *  \param threshold minimum level printed
	 */
	void inline DumpErrors(MsgType const &threshold) {
		DumpErrors(std::cerr, threshold);
	}

        // mvo: we do this instead of using a default parameter in the
        //      previous declaration to avoid a (subtle) API break for
        //      e.g. sigc++ and mem_fun0
	/** \brief dumps the messages of type WARNING or higher to std::cerr
	 *
	 *  Note that all messages are discarded, displayed or not.
	 *
	 */
	void inline DumpErrors() {
                DumpErrors(WARNING);
	}

	/** \brief put the current Messages into the stack
	 *
	 *  All "old" messages will be pushed into a stack to
	 *  them later back, but for now the Message query will be
	 *  empty and performs as no messages were present before.
	 *
	 * The stack can be as deep as you want - all stack operations
	 * will only operate on the last element in the stack.
	 */
	void PushToStack();

	/** \brief throw away all current messages */
	void RevertToStack();

	/** \brief merge current and stack together */
	void MergeWithStack();

	/** \brief return the deep of the stack */
	size_t StackCount() const APT_PURE {
		return Stacks.size();
	}

	GlobalError();
									/*}}}*/
private:								/*{{{*/
	struct Item {
		std::string Text;
		MsgType Type;

		Item(char const *Text, MsgType const &Type) :
			Text(Text), Type(Type) {};

		APT_HIDDEN friend std::ostream &operator<<(std::ostream &out, Item i);
	};

	APT_HIDDEN friend std::ostream &operator<<(std::ostream &out, Item i);

	std::list<Item> Messages;
	bool PendingFlag;

	struct MsgStack {
		std::list<Item> Messages;
		bool const PendingFlag;

		MsgStack(std::list<Item> const &Messages, bool const &Pending) :
			 Messages(Messages), PendingFlag(Pending) {};
	};

	std::list<MsgStack> Stacks;
									/*}}}*/
};
									/*}}}*/

// The 'extra-ansi' syntax is used to help with collisions. 
APT_PUBLIC GlobalError *_GetErrorObj();
static struct {
	inline GlobalError* operator ->() { return _GetErrorObj(); }
} _error APT_UNUSED;

#endif
