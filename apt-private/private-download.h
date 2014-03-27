#ifndef APT_PRIVATE_DOWNLOAD_H
#define APT_PRIVATE_DOWNLOAD_H

#include <apt-pkg/macros.h>

class pkgAcquire;

APT_PUBLIC bool CheckAuth(pkgAcquire& Fetcher, bool const PromptUser);
APT_PUBLIC bool AcquireRun(pkgAcquire &Fetcher, int const PulseInterval, bool * const Failure, bool * const TransientNetworkFailure);

#endif
