#include <apt-pkg/error.h>

#include "assert.h"
#include <string>
#include <errno.h>

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

	errno = 0;
	equals(_error->Errno("errno", "%s horrible %s %d times", "Something", "happend", 2), false);
	equals(_error->empty(), false);
	equals(_error->PendingError(), true);
	equals(_error->PopMessage(text), true);
	equals(_error->PendingError(), false);
	equals(text, "Something horrible happend 2 times - errno (0: Success)");
	equals(_error->empty(), true);

	std::string longText;
	for (size_t i = 0; i < 500; ++i)
		longText.append("a");
	equals(_error->Error("%s horrible %s %d times", longText.c_str(), "happend", 2), false);
	equals(_error->PopMessage(text), true);
	equals(text, std::string(longText).append(" horrible happend 2 times"));

	equals(_error->Errno("errno", "%s horrible %s %d times", longText.c_str(), "happend", 2), false);
	equals(_error->PopMessage(text), true);
	equals(text, std::string(longText).append(" horrible happend 2 times - errno (0: Success)"));

	equals(_error->Warning("Репозиторий не обновлён и будут %d %s", 4, "test"), false);
	equals(_error->PopMessage(text), false);
	equals(text, "Репозиторий не обновлён и будут 4 test");

	longText.clear();
	for (size_t i = 0; i < 50; ++i)
		longText.append("РезийбёбAZ");
	equals(_error->Warning(longText.c_str()), false);
	equals(_error->PopMessage(text), false);
	equals(text, longText);

	return 0;
}
