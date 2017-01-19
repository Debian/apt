#ifndef APT_PRIVATE_UTILS_H
#define APT_PRIVATE_UTILS_H

#include <string>

bool DisplayFileInPager(std::string const &filename);
bool EditFileInSensibleEditor(std::string const &filename);
time_t GetSecondsSinceEpoch();

#endif
