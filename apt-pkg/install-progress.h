#ifndef PKGLIB_IPROGRESS_H
#define PKGLIB_IPROGRESS_H

#include <apt-pkg/macros.h>

#include <csignal>
#include <string>
#include <vector>
#include <unistd.h>

namespace APT {
namespace Progress {

 class PackageManager;
 APT_PUBLIC PackageManager* PackageManagerProgressFactory();

 class APT_PUBLIC PackageManager
 {
 private:
    /** \brief dpointer placeholder */
    void * const d;

 protected:
    std::string progress_str;
    float percentage;
    int last_reported_progress;

 public:
    PackageManager();
    virtual ~PackageManager();

    /* Global Start/Stop */
    virtual void Start(int /*child_pty*/=-1) {};
    virtual void Stop() {};

    /* When dpkg is invoked (may happen multiple times for each
     * install/remove block
    */
    virtual void StartDpkg() {};

    virtual pid_t fork() {return ::fork(); };

    virtual void Pulse() {};
    virtual long GetPulseInterval() {
         return 50000000;
    };

    virtual bool StatusChanged(std::string PackageName,
                               unsigned int StepsDone,
                               unsigned int TotalSteps,
                               std::string HumanReadableAction);
    virtual void Error(std::string /*PackageName*/,
                       unsigned int /*StepsDone*/,
                       unsigned int /*TotalSteps*/,
                       std::string /*ErrorMessage*/) {}
    virtual void ConffilePrompt(std::string /*PackageName*/,
                                unsigned int /*StepsDone*/,
                                unsigned int /*TotalSteps*/,
                                std::string /*ConfMessage*/) {}
 };

 class APT_PUBLIC PackageManagerProgressFd : public PackageManager
 {
    void * const d;
 protected:
    int OutStatusFd;
    int StepsDone;
    int StepsTotal;
    void WriteToStatusFd(std::string msg);

 public:
    explicit PackageManagerProgressFd(int progress_fd);
    virtual ~PackageManagerProgressFd();

    virtual void StartDpkg() APT_OVERRIDE;
    virtual void Stop() APT_OVERRIDE;

    virtual bool StatusChanged(std::string PackageName,
                               unsigned int StepsDone,
                               unsigned int TotalSteps,
                               std::string HumanReadableAction) APT_OVERRIDE;
    virtual void Error(std::string PackageName,
                       unsigned int StepsDone,
                       unsigned int TotalSteps,
                          std::string ErrorMessage) APT_OVERRIDE;
    virtual void ConffilePrompt(std::string PackageName,
                                unsigned int StepsDone,
                                unsigned int TotalSteps,
                                   std::string ConfMessage) APT_OVERRIDE;

 };

 class APT_PUBLIC PackageManagerProgressDeb822Fd : public PackageManager
 {
    void * const d;
 protected:
    int OutStatusFd;
    int StepsDone;
    int StepsTotal;
    void WriteToStatusFd(std::string msg);

 public:
    explicit PackageManagerProgressDeb822Fd(int progress_fd);
    virtual ~PackageManagerProgressDeb822Fd();

    virtual void StartDpkg() APT_OVERRIDE;
    virtual void Stop() APT_OVERRIDE;

    virtual bool StatusChanged(std::string PackageName,
                               unsigned int StepsDone,
                               unsigned int TotalSteps,
                               std::string HumanReadableAction) APT_OVERRIDE;
    virtual void Error(std::string PackageName,
                       unsigned int StepsDone,
                       unsigned int TotalSteps,
                          std::string ErrorMessage) APT_OVERRIDE;
    virtual void ConffilePrompt(std::string PackageName,
                                unsigned int StepsDone,
                                unsigned int TotalSteps,
                                   std::string ConfMessage) APT_OVERRIDE;
 };

 class APT_PUBLIC PackageManagerFancy : public PackageManager
 {
    void * const d;
 private:
    APT_HIDDEN static void staticSIGWINCH(int);
    static std::vector<PackageManagerFancy*> instances;
    static sighandler_t SIGWINCH_orig;
    static volatile sig_atomic_t SIGWINCH_flag;
    APT_HIDDEN void CheckSIGWINCH();
    APT_HIDDEN bool DrawStatusLine();

 protected:
    void SetupTerminalScrollArea(int nr_rows);
    void HandleSIGWINCH(int); // for abi compatibility, do not use

    typedef struct {
       int rows;
       int columns;
    } TermSize;
    TermSize GetTerminalSize();

    sighandler_t old_SIGWINCH; // for abi compatibility, do not use
    int child_pty;

 public:
    PackageManagerFancy();
    virtual ~PackageManagerFancy();
    virtual void Pulse() APT_OVERRIDE;
    virtual void Start(int child_pty=-1) APT_OVERRIDE;
    virtual void Stop() APT_OVERRIDE;
    virtual bool StatusChanged(std::string PackageName,
                               unsigned int StepsDone,
                               unsigned int TotalSteps,
                               std::string HumanReadableAction) APT_OVERRIDE;

    // return a progress bar of the given size for the given progress 
    // percent between 0.0 and 1.0 in the form "[####...]"
    static std::string GetTextProgressStr(float percent, int OutputSize);
 };

 class APT_PUBLIC PackageManagerText : public PackageManager
 {
    void * const d;
 public:
    virtual bool StatusChanged(std::string PackageName,
                               unsigned int StepsDone,
                               unsigned int TotalSteps,
                               std::string HumanReadableAction) APT_OVERRIDE;

    PackageManagerText();
    virtual ~PackageManagerText();
 };


} // namespace Progress
} // namespace APT

#endif
