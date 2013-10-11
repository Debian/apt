#ifndef PKGLIB_IPROGRESS_H
#define PKGLIB_IPROGRSS_H


#include <apt-pkg/packagemanager.h>

namespace APT {
namespace Progress {


 class PackageManager
 {
 private:
    /** \brief dpointer placeholder */
    void *d;

 public:
    virtual ~PackageManager() {};

    virtual void Started() {};
    virtual void Finished() {};
    
    virtual void StatusChanged(std::string PackageName, 
                               unsigned int StepsDone,
                               unsigned int TotalSteps) {};
 };

 class PackageManagerFancy : public PackageManager
 {
 protected:
    int last_reported_progress;
    int nr_terminal_rows;
 public:
    PackageManagerFancy();
    virtual void Started();
    virtual void Finished();
    virtual void StatusChanged(std::string PackageName, 
                               unsigned int StepsDone,
                               unsigned int TotalSteps);
 };

 class PackageManagerText : public PackageManager
 {
 protected:
    int last_reported_progress;

 public:
    PackageManagerText() : last_reported_progress(0) {};
    virtual void StatusChanged(std::string PackageName, 
                               unsigned int StepsDone,
                               unsigned int TotalSteps);

 };


}; // namespace Progress
}; // namespace APT

#endif
