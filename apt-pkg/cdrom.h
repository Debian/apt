#ifndef PKGLIB_CDROM_H
#define PKGLIB_CDROM_H

#include <apt-pkg/macros.h>

#include <string>
#include <vector>

#include <stddef.h>


class Configuration;
class OpProgress;

class pkgCdromStatus							/*{{{*/
{
   void * const d;
 protected:
   int totalSteps;

 public:
   pkgCdromStatus();
   virtual ~pkgCdromStatus();

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

   pkgCdrom();
   virtual ~pkgCdrom();

 private:
   void * const d;

   APT_HIDDEN bool MountAndIdentCDROM(Configuration &Database, std::string &CDROM,
	 std::string &ident, pkgCdromStatus * const log, bool const interactive);
   APT_HIDDEN bool UnmountCDROM(std::string const &CDROM, pkgCdromStatus * const log);
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
   void * const d;

 protected:
   // old libudev dlopen structure, not used anymore, but can't break ABI
   void *_padding;
   struct _padding *(*_padding__new)(void);
   int (*_padding__enumerate_add_match_property)(struct _padding_enumerate *_padding_enumerate, const char *property, const char *value);
   int (*_padding__enumerate_scan_devices)(struct _padding_enumerate *_padding_enumerate);
   struct _padding_list_entry *(*_padding__enumerate_get_list_entry)(struct _padding_enumerate *_padding_enumerate);
   struct _padding_device *(*_padding__device_new_from_syspath)(struct _padding *_padding, const char *syspath);
   struct _padding *(*_padding__enumerate_get__padding)(struct _padding_enumerate *_padding_enumerate);
   const char *(*_padding__list_entry_get_name)(struct _padding_list_entry *list_entry);
   const char *(*_padding__device_get_devnode)(struct _padding_device *_padding_device);
   struct _padding_enumerate *(*_padding__enumerate_new)(struct _padding *_padding);
   struct _padding_list_entry *(*_padding__list_entry_get_next)(struct _padding_list_entry *list_entry);
   const char *(*_padding__device_get_property_value)(struct _padding_device *_padding_device, const char *key);
   int (*_padding__enumerate_add_match_sysattr)(struct _padding_enumerate *_padding_enumerate, const char *property, const char *value);
   // end lib_padding dlopen
   
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
