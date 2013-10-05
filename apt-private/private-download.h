#ifndef APT_PRIVATE_DOWNLOAD_H
#define APT_PRIVATE_DOWNLOAD_H

#include <apt-pkg/acquire.h>

bool CheckAuth(pkgAcquire& Fetcher, bool const PromptUser);
bool AcquireRun(pkgAcquire &Fetcher, int const PulseInterval, bool * const Failure, bool * const TransientNetworkFailure);

#endif
