// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: error.h,v 1.8 2001/05/07 05:06:52 jgg Exp $
/* ######################################################################
   
   Global Erorr Class - Global error mechanism

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

#include <stdarg.h>

class GlobalError							/*{{{*/
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
	bool FatalE(const char *Function,const char *Description,...) __like_printf(3) __cold;

	/** \brief add an Error message with errno to the list
	 *
	 *  \param Function name of the function generating the error
	 *  \param Description format string for the error message
	 *
	 *  \return \b false
	 */
	bool Errno(const char *Function,const char *Description,...) __like_printf(3) __cold;

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
	bool WarningE(const char *Function,const char *Description,...) __like_printf(3) __cold;

	/** \brief add a notice message with errno to the list
	 *
	 *  \param Function name of the function generating the error
	 *  \param Description format string for the error message
	 *
	 *  \return \b false
	 */
	bool NoticeE(const char *Function,const char *Description,...) __like_printf(3) __cold;

	/** \brief add a debug message with errno to the list
	 *
	 *  \param Function name of the function generating the error
	 *  \param Description format string for the error message
	 *
	 *  \return \b false
	 */
	bool DebugE(const char *Function,const char *Description,...) __like_printf(3) __cold;

	/** \brief adds an errno message with the given type
	 *
	 * \param type of the error message
	 * \param Function which failed
	 * \param Description of the error
	 */
	bool InsertErrno(MsgType const &type, const char* Function,
			 const char* Description,...) __like_printf(4) __cold;

	/** \brief add an fatal error message to the list
	 *
	 *  Most of the stuff we consider as "error" is also "fatal" for
	 *  the user as the application will not have the expected result,
	 *  but a fatal message here means that it gets printed directly
	 *  to stderr in addiction to adding it to the list as the error
	 *  leads sometimes to crashes and a maybe duplicated message
	 *  is better than "Segfault" as the only displayed text
	 *
	 *  \param Description Format string for the fatal error message.
	 *
	 *  \return \b false
	 */
	bool Fatal(const char *Description,...) __like_printf(2) __cold;

	/** \brief add an Error message to the list
	 *
	 *  \param Description Format string for the error message.
	 *
	 *  \return \b false
	 */
	bool Error(const char *Description,...) __like_printf(2) __cold;

	/** \brief add a warning message to the list
	 *
	 *  A warning should be considered less severe than an error and
	 *  may be ignored by the client.
	 *
	 *  \param Description Format string for the message
	 *
	 *  \return \b false
	 */
	bool Warning(const char *Description,...) __like_printf(2) __cold;

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
	bool Notice(const char *Description,...) __like_printf(2) __cold;

	/** \brief add a debug message to the list
	 *
	 *  \param Description Format string for the message
	 *
	 *  \return \b false
	 */
	bool Debug(const char *Description,...) __like_printf(2) __cold;

	/** \brief adds an error message with the given type
	 *
	 * \param type of the error message
	 * \param Description of the error
	 */
	bool Insert(MsgType const &type, const char* Description,...) __like_printf(3) __cold;

	/** \brief is an error in the list?
	 *
	 *  \return \b true if an error is included in the list, \b false otherwise
	 */
	inline bool PendingError() const {return PendingFlag;};

	/** \brief is the list empty?
	 *
	 *  The default checks if the list is empty or contains only notices,
	 *  if you want to check if also no notices happend set the parameter
	 *  flag to \b false.
	 *
	 *  \param WithoutNotice does notices count, default is \b true, so no
	 *
	 *  \return \b true if an the list is empty, \b false otherwise
	 */
	bool empty(MsgType const &trashhold = WARNING) const;

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
	 *  Note that all messages are discarded, also the notices
	 *  displayed or not.
	 *
	 *  \param[out] out output stream to write the messages in
	 *  \param threshold minimim level considered
         *  \param mergeStack 
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
	size_t StackCount() const {
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

		friend std::ostream& operator<< (std::ostream &out, Item i) {
			switch(i.Type) {
			case FATAL:
			case ERROR: out << "E"; break;
			case WARNING: out << "W"; break;
			case NOTICE: out << "N"; break;
			case DEBUG: out << "D"; break;
			}
			return out << ": " << i.Text;
		}
	};

	std::list<Item> Messages;
	bool PendingFlag;

	struct MsgStack {
		std::list<Item> const Messages;
		bool const PendingFlag;

		MsgStack(std::list<Item> const &Messages, bool const &Pending) :
			 Messages(Messages), PendingFlag(Pending) {};
	};

	std::list<MsgStack> Stacks;

	bool InsertErrno(MsgType type, const char* Function,
			 const char* Description, va_list &args,
			 int const errsv, size_t &msgSize);
	bool Insert(MsgType type, const char* Description,
			 va_list &args, size_t &msgSize);
									/*}}}*/
};
									/*}}}*/

// The 'extra-ansi' syntax is used to help with collisions. 
GlobalError *_GetErrorObj();
#define _error _GetErrorObj()

#endif
