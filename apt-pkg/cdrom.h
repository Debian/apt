#ifndef PKGLIB_CDROM_H
#define PKGLIB_CDROM_H

#include<apt-pkg/init.h>
#include<string>
#include<vector>


using namespace std;

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
   virtual void Update(string text="", int current=0) = 0;
   
   // ask for cdrom insert
   virtual bool ChangeCdrom() = 0;
   // ask for cdrom name
   virtual bool AskCdromName(string &Name) = 0;
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


   bool FindPackages(string CD,
		     vector<string> &List,
		     vector<string> &SList, 
		     vector<string> &SigList,
		     vector<string> &TransList,
		     string &InfoDir, pkgCdromStatus *log,
		     unsigned int Depth = 0);
   bool DropBinaryArch(vector<string> &List);
   bool DropRepeats(vector<string> &List,const char *Name);
   void ReduceSourcelist(string CD,vector<string> &List);
   bool WriteDatabase(Configuration &Cnf);
   bool WriteSourceList(string Name,vector<string> &List,bool Source);
   int Score(string Path);

 public:
   bool Ident(string &ident, pkgCdromStatus *log);
   bool Add(pkgCdromStatus *log);
};
									/*}}}*/


// class that uses libudev to find cdrom/removable devices dynamically
struct CdromDevice							/*{{{*/
{
   string DeviceName;
   bool Mounted;
   string MountPath;
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
#if 0 // FIXME: uncomment on next ABI break
   int (*udev_enumerate_add_match_sysattr)(struct udev_enumerate *udev_enumerate, const char *property, const char *value);
#endif 
   // end libudev dlopen
   
 public:
   pkgUdevCdromDevices();
   virtual ~pkgUdevCdromDevices();

   // try to open 
   bool Dlopen();

   // this is the new interface
   vector<CdromDevice> ScanForRemovable(bool CdromOnly);
   // FIXME: compat with the old interface/API/ABI only
   vector<CdromDevice> Scan();

};
									/*}}}*/

#endif
