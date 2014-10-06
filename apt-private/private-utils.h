#ifndef APT_PRIVATE_UTILS_H
#define APT_PRIVATE_UTILS_H

#include <apt-pkg/macros.h>

#include <string>

APT_PUBLIC void DisplayFileInPager(std::string const &filename);
APT_PUBLIC void EditFileInSensibleEditor(std::string const &filename);

#endif
