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

 protected:
    std::string progress_str;
    float percentage;
    int last_reported_progress;

 public:
    PackageManager() : percentage(0.0), last_reported_progress(0) {};
    virtual ~PackageManager() {};

    virtual void Started() {};
    virtual void Finished() {};
    
    virtual bool StatusChanged(std::string PackageName, 
                               unsigned int StepsDone,
                               unsigned int TotalSteps);
 };

 class PackageManagerFancy : public PackageManager
 {
 protected:
    int nr_terminal_rows;
    void SetupTerminalScrollArea(int nr_rows);

 public:
    PackageManagerFancy();
    virtual void Started();
    virtual void Finished();
    virtual bool StatusChanged(std::string PackageName, 
                               unsigned int StepsDone,
                               unsigned int TotalSteps);
 };

 class PackageManagerText : public PackageManager
 {
 public:
    virtual bool StatusChanged(std::string PackageName, 
                               unsigned int StepsDone,
                               unsigned int TotalSteps);

 };


}; // namespace Progress
}; // namespace APT

#endif
