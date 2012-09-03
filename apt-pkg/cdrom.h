#ifndef PKGLIB_CDROM_H
#define PKGLIB_CDROM_H

#include<string>
#include<vector>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/init.h>
using namespace std;
#endif

class Configuration;
class OpProgress;

class pkgCdromStatus							/*{{{*/
{
 protected:
   int totalSteps;

 public:
   pkgCdromStatus() {};
   virtual ~pkgCdromStatus() {};

   // total steps
   virtual void SetTotal(int total) { totalSteps = total; };
   // update steps, will be called regularly as a "pulse"
   virtual void Update(std::string text="", int current=0) = 0;
   
   // ask for cdrom insert
   virtual bool ChangeCdrom() = 0;
   // ask for cdrom name
   virtual bool AskCdromName(std::string &Name) = 0;
   // Progress indicator for the Index rewriter
   virtual OpProgress* GetOpProgress() {return NULL; };
};
									/*}}}*/
class pkgCdrom								/*{{{*/
{
 protected:
   enum {
      STEP_PREPARE = 1,
      STEP_UNMOUNT,
      STEP_WAIT,
      STEP_MOUNT,
      STEP_IDENT,
      STEP_SCAN,
      STEP_COPY,
      STEP_WRITE,
      STEP_UNMOUNT3,
      STEP_LAST
   };


   bool FindPackages(std::string CD,
		     std::vector<std::string> &List,
		     std::vector<std::string> &SList, 
		     std::vector<std::string> &SigList,
		     std::vector<std::string> &TransList,
		     std::string &InfoDir, pkgCdromStatus *log,
		     unsigned int Depth = 0);
   bool DropBinaryArch(std::vector<std::string> &List);
   bool DropRepeats(std::vector<std::string> &List,const char *Name);
   bool DropTranslation(std::vector<std::string> &List);
   void ReduceSourcelist(std::string CD,std::vector<std::string> &List);
   bool WriteDatabase(Configuration &Cnf);
   bool WriteSourceList(std::string Name,std::vector<std::string> &List,bool Source);
   int Score(std::string Path);

 public:
   bool Ident(std::string &ident, pkgCdromStatus *log);
   bool Add(pkgCdromStatus *log);
};
									/*}}}*/


// class that uses libudev to find cdrom/removable devices dynamically
struct CdromDevice							/*{{{*/
{
   std::string DeviceName;
   bool Mounted;
   std::string MountPath;
};
									/*}}}*/
class pkgUdevCdromDevices						/*{{{*/
{
 protected:
   // libudev dlopen stucture
   void *libudev_handle;
   struct udev* (*udev_new)(void);
   int (*udev_enumerate_add_match_property)(struct udev_enumerate *udev_enumerate, const char *property, const char *value);
   int (*udev_enumerate_scan_devices)(struct udev_enumerate *udev_enumerate);
   struct udev_list_entry* (*udev_enumerate_get_list_entry)(struct udev_enumerate *udev_enumerate);
   struct udev_device* (*udev_device_new_from_syspath)(struct udev *udev, const char *syspath);
   struct udev* (*udev_enumerate_get_udev)(struct udev_enumerate *udev_enumerate);
   const char* (*udev_list_entry_get_name)(struct udev_list_entry *list_entry);
   const char* (*udev_device_get_devnode)(struct udev_device *udev_device);
   struct udev_enumerate *(*udev_enumerate_new) (struct udev *udev);
   struct udev_list_entry *(*udev_list_entry_get_next)(struct udev_list_entry *list_entry);
   const char* (*udev_device_get_property_value)(struct udev_device *udev_device, const char *key);
   int (*udev_enumerate_add_match_sysattr)(struct udev_enumerate *udev_enumerate, const char *property, const char *value);
   // end libudev dlopen
   
 public:
   pkgUdevCdromDevices();
   virtual ~pkgUdevCdromDevices();

   // try to open 
   bool Dlopen();

   // convenience interface, this will just call ScanForRemovable
   // with "APT::cdrom::CdromOnly"
   std::vector<CdromDevice> Scan();

   std::vector<CdromDevice> ScanForRemovable(bool CdromOnly);
};
									/*}}}*/

#endif
