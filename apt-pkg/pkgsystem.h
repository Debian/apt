// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   System - Abstraction for running on different systems.
   
   Instances of this class can be thought of as factories or meta-classes
   for a variety of more specialized classes. Together this class and 
   it's specialized offspring completely define the environment and how
   to access resources for a specific system. There are several sub
   areas that are all orthogonal - each system has a unique combination of
   these sub areas:
       - Versioning. Different systems have different ideas on versions.
         Within a system all sub classes must follow the same versioning 
         rules.
       - Local tool locking to prevent multiple tools from accessing the
         same database.
       - Candidate Version selection policy - this is probably almost always
         managed using a standard APT class
       - Actual Package installation 
         * Indication of what kind of binary formats are supported
       - Selection of local 'status' indexes that make up the pkgCache.
      
   It is important to note that the handling of index files is not a 
   function of the system. Index files are handled through a separate
   abstraction - the only requirement is that the index files have the
   same idea of versioning as the target system.
   
   Upon startup each supported system instantiates an instance of the
   pkgSystem class (using a global constructor) which will make itself
   available to the main APT init routine. That routine will select the
   proper system and make it the global default.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PKGSYSTEM_H
#define PKGLIB_PKGSYSTEM_H

#include <apt-pkg/pkgcache.h>

#include <vector>


class pkgDepCache;
class pkgPackageManager;
class pkgVersioningSystem;
class Configuration;
class pkgIndexFile;
class OpProgress;

class pkgSystemPrivate;
class APT_PUBLIC pkgSystem
{
   public:

   // Global list of supported systems
   static pkgSystem **GlobalList;
   static unsigned long GlobalListLen;
   static pkgSystem *GetSystem(const char *Label);
   
   const char * const Label;
   pkgVersioningSystem * const VS;
   
   /* Prevent other programs from touching shared data not covered by
      other locks (cache or state locks) */
   virtual bool Lock(OpProgress *const Progress = nullptr) = 0;
   virtual bool UnLock(bool NoErrors = false) = 0;
   
   /* Various helper classes to interface with specific bits of this
      environment */
   virtual pkgPackageManager *CreatePM(pkgDepCache *Cache) const = 0;

   /* Load environment specific configuration and perform any other setup
      necessary */
   virtual bool Initialize(Configuration &/*Cnf*/) {return true;};
   
   /* Type is some kind of Globally Unique way of differentiating
      archive file types.. */
   virtual bool ArchiveSupported(const char *Type) = 0;

   // Return a list of system index files..
   virtual bool AddStatusFiles(std::vector<pkgIndexFile *> &List) = 0;

   virtual bool FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const = 0;

   /* Evaluate how 'right' we are for this system based on the filesystem
      etc.. */
   virtual signed Score(Configuration const &/*Cnf*/) {
      return 0;
   };

   //FIXME: these methods should be virtual
   /** does this system has support for MultiArch?
    *
    * Systems supporting only single arch (not systems which are single arch)
    * are considered legacy systems and support for it will likely degrade over
    * time.
    *
    * The default implementation returns always \b true.
    *
    * @return \b true if the system supports MultiArch, \b false if not.
    */
   virtual bool MultiArchSupported() const = 0;
   /** architectures supported by this system
    *
    * A MultiArch capable system might be configured to use
    * this capability.
    *
    * @return a list of all architectures (native + foreign) configured
    * for on this system (aka: which can be installed without force)
    */
   virtual std::vector<std::string> ArchitecturesSupported() const  = 0;

   APT_HIDDEN void SetVersionMapping(map_id_t const in, map_id_t const out);
   APT_HIDDEN map_id_t GetVersionMapping(map_id_t const in) const;

   pkgSystem(char const * const Label, pkgVersioningSystem * const VS);
   virtual ~pkgSystem();


   /* companions to Lock()/UnLock
    *
    * These functions can be called prior to calling dpkg to release an inner
    * lock without releasing the overall outer lock, so that dpkg can run
    * correctly but no other APT instance can acquire the system lock.
    */
   virtual bool LockInner(OpProgress *const Progress = 0, int timeOutSec = 0) = 0;
   virtual bool UnLockInner(bool NoErrors = false)  = 0;
   /// checks if the system is currently locked
   virtual bool IsLocked() = 0;
   private:
   pkgSystemPrivate * const d;
};

// The environment we are operating in.
APT_PUBLIC extern pkgSystem *_system;

#endif
