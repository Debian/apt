#include <apt-pkg/error.h>

#include "assert.h"
#include <string>

int main(int argc,char *argv[])
{
	equals(_error->empty(), true);
	equals(_error->PendingError(), false);
	equals(_error->Notice("%s Notice", "A"), false);
	equals(_error->empty(), true);
	equals(_error->empty(GlobalError::DEBUG), false);
	equals(_error->PendingError(), false);
	equals(_error->Error("%s horrible %s %d times", "Something", "happend", 2), false);
	equals(_error->PendingError(), true);
	std::string text;
	equals(_error->PopMessage(text), false);
	equals(_error->PendingError(), true);
	equals(text, "A Notice");
	equals(_error->PopMessage(text), true);
	equals(text, "Something horrible happend 2 times");
	equals(_error->empty(GlobalError::DEBUG), true);
	equals(_error->PendingError(), false);
	equals(_error->Error("%s horrible %s %d times", "Something", "happend", 2), false);
	equals(_error->PendingError(), true);
	equals(_error->empty(GlobalError::FATAL), false);
	_error->Discard();

	equals(_error->empty(), true);
	equals(_error->PendingError(), false);
	equals(_error->Notice("%s Notice", "A"), false);
	equals(_error->Error("%s horrible %s %d times", "Something", "happend", 2), false);
	equals(_error->PendingError(), true);
	equals(_error->empty(GlobalError::NOTICE), false);
	_error->PushToStack();
	equals(_error->empty(GlobalError::NOTICE), true);
	equals(_error->PendingError(), false);
	equals(_error->Warning("%s Warning", "A"), false);
	equals(_error->empty(GlobalError::ERROR), true);
	equals(_error->PendingError(), false);
	_error->RevertToStack();
	equals(_error->empty(GlobalError::ERROR), false);
	equals(_error->PendingError(), true);
	equals(_error->PopMessage(text), false);
	equals(_error->PendingError(), true);
	equals(text, "A Notice");
	equals(_error->PopMessage(text), true);
	equals(text, "Something horrible happend 2 times");
	equals(_error->PendingError(), false);
	equals(_error->empty(), true);

	equals(_error->Notice("%s Notice", "A"), false);
	equals(_error->Error("%s horrible %s %d times", "Something", "happend", 2), false);
	equals(_error->PendingError(), true);
	equals(_error->empty(GlobalError::NOTICE), false);
	_error->PushToStack();
	equals(_error->empty(GlobalError::NOTICE), true);
	equals(_error->PendingError(), false);
	equals(_error->Warning("%s Warning", "A"), false);
	equals(_error->empty(GlobalError::ERROR), true);
	equals(_error->PendingError(), false);
	_error->MergeWithStack();
	equals(_error->empty(GlobalError::ERROR), false);
	equals(_error->PendingError(), true);
	equals(_error->PopMessage(text), false);
	equals(_error->PendingError(), true);
	equals(text, "A Notice");
	equals(_error->PopMessage(text), true);
	equals(text, "Something horrible happend 2 times");
	equals(_error->PendingError(), false);
	equals(_error->empty(), false);
	equals(_error->PopMessage(text), false);
	equals(text, "A Warning");
	equals(_error->empty(), true);

	return 0;
}
