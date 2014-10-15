#ifndef APT_PRIVATE_DOWNLOAD_H
#define APT_PRIVATE_DOWNLOAD_H

#include <apt-pkg/macros.h>

#include <string>

class pkgAcquire;

APT_PUBLIC bool CheckDropPrivsMustBeDisabled(pkgAcquire &Fetcher);

// Check if all files in the fetcher are authenticated
APT_PUBLIC bool CheckAuth(pkgAcquire& Fetcher, bool const PromptUser);

// show a authentication warning prompt and return true if the system
// should continue
APT_PUBLIC bool AuthPrompt(std::string const &UntrustedList, bool const PromptUser);

APT_PUBLIC bool AcquireRun(pkgAcquire &Fetcher, int const PulseInterval, bool * const Failure, bool * const TransientNetworkFailure);

#endif
