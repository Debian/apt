// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: extract.cc,v 1.6.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   Archive Extraction Directory Stream
   
   Extraction for each file is a bit of an involved process. Each object
   undergoes an atomic backup, overwrite, erase sequence. First the
   object is unpacked to '.dpkg.new' then the original is hardlinked to
   '.dpkg.tmp' and finally the new object is renamed to overwrite the old
   one. From an external perspective the file never ceased to exist.
   After the archive has been sucessfully unpacked the .dpkg.tmp files 
   are erased. A failure causes all the .dpkg.tmp files to be restored.
   
   Decisions about unpacking go like this:
      - Store the original filename in the file listing
      - Resolve any diversions that would effect this file, all checks
        below apply to the diverted name, not the real one.
      - Resolve any symlinked configuration files.
      - If the existing file does not exist then .dpkg-tmp is checked for.
        [Note, this is reduced to only check if a file was expected to be
         there]
      - If the existing link/file is not a directory then it is replaced
        irregardless
      - If the existing link/directory is being replaced by a directory then
        absolutely nothing happens.
      - If the existing link/directory is being replaced by a link then
        absolutely nothing happens.
      - If the existing link/directory is being replaced by a non-directory
        then this will abort if the package is not the sole owner of the
        directory. [Note, this is changed to not happen if the directory
        non-empty - that is, it only includes files that are part of this
        package - prevents removing user files accidentally.]
      - If the non-directory exists in the listing database and it
        does not belong to the current package then an overwrite condition
        is invoked. 
   
   As we unpack we record the file list differences in the FL cache. If
   we need to unroll the the FL cache knows which files have been unpacked
   and can undo. When we need to erase then it knows which files have not 
   been unpacked.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/extract.h>
#include <apt-pkg/error.h>
#include <apt-pkg/debversion.h>

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <iostream>
#include <apti18n.h>
									/*}}}*/
using namespace std;

static const char *TempExt = "dpkg-tmp";
//static const char *NewExt = "dpkg-new";

// Extract::pkgExtract - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgExtract::pkgExtract(pkgFLCache &FLCache,pkgCache::VerIterator Ver) : 
                       FLCache(FLCache), Ver(Ver)
{
   FLPkg = FLCache.GetPkg(Ver.ParentPkg().Name(),true);
   if (FLPkg.end() == true)
      return;
   Debug = true;
}
									/*}}}*/
// Extract::DoItem - Handle a single item from the stream		/*{{{*/
// ---------------------------------------------------------------------
/* This performs the setup for the extraction.. */
bool pkgExtract::DoItem(Item &Itm,int &Fd)
{
   char Temp[sizeof(FileName)];
   
   /* Strip any leading/trailing /s from the filename, then copy it to the
      temp buffer and re-apply the leading / We use a class variable
      to store the new filename for use by the three extraction funcs */
   char *End = FileName+1;
   const char *I = Itm.Name;
   for (; *I != 0 && *I == '/'; I++);
   *FileName = '/';
   for (; *I != 0 && End < FileName + sizeof(FileName); I++, End++)
      *End = *I;
   if (End + 20 >= FileName + sizeof(FileName))
      return _error->Error(_("The path %s is too long"),Itm.Name);   
   for (; End > FileName && End[-1] == '/'; End--);
   *End = 0;
   Itm.Name = FileName;
   
   /* Lookup the file. Nde is the file [group] we are going to write to and
      RealNde is the actual node we are manipulating. Due to diversions
      they may be entirely different. */
   pkgFLCache::NodeIterator Nde = FLCache.GetNode(Itm.Name,End,0,false,false);
   pkgFLCache::NodeIterator RealNde = Nde;
      
   // See if the file is already in the file listing
   unsigned long FileGroup = RealNde->File;
   for (; RealNde.end() == false && FileGroup == RealNde->File; RealNde++)
      if (RealNde.RealPackage() == FLPkg)
	 break;

   // Nope, create an entry
   if (RealNde.end() == true)
   {
      RealNde = FLCache.GetNode(Itm.Name,End,FLPkg.Offset(),true,false);
      if (RealNde.end() == true)
	 return false;
      RealNde->Flags |= pkgFLCache::Node::NewFile;
   }

   /* Check if this entry already was unpacked. The only time this should 
      ever happen is if someone has hacked tar to support capabilities, in
      which case this needs to be modified anyhow.. */
   if ((RealNde->Flags & pkgFLCache::Node::Unpacked) ==
       pkgFLCache::Node::Unpacked)
      return _error->Error(_("Unpacking %s more than once"),Itm.Name);
   
   if (Nde.end() == true)
      Nde = RealNde;

   /* Consider a diverted file - We are not permitted to divert directories,
      but everything else is fair game (including conf files!) */
   if ((Nde->Flags & pkgFLCache::Node::Diversion) != 0)
   {
      if (Itm.Type == Item::Directory)
	 return _error->Error(_("The directory %s is diverted"),Itm.Name);

      /* A package overwriting a diversion target is just the same as 
         overwriting a normally owned file and is checked for below in
	 the overwrites mechanism */

      /* If this package is trying to overwrite the target of a diversion, 
         that is never, ever permitted */
      pkgFLCache::DiverIterator Div = Nde.Diversion();
      if (Div.DivertTo() == Nde)
	 return _error->Error(_("The package is trying to write to the "
			      "diversion target %s/%s"),Nde.DirN(),Nde.File());
      
      // See if it is us and we are following it in the right direction
      if (Div->OwnerPkg != FLPkg.Offset() && Div.DivertFrom() == Nde)
      {
	 Nde = Div.DivertTo();
	 End = FileName + snprintf(FileName,sizeof(FileName)-20,"%s/%s",
				   Nde.DirN(),Nde.File());
	 if (End <= FileName)
	    return _error->Error(_("The diversion path is too long"));
      }      
   }
   
   // Deal with symlinks and conf files
   if ((RealNde->Flags & pkgFLCache::Node::NewConfFile) == 
       pkgFLCache::Node::NewConfFile)
   {
      string Res = flNoLink(Itm.Name);
      if (Res.length() > sizeof(FileName))
	 return _error->Error(_("The path %s is too long"),Res.c_str());
      if (Debug == true)
	 clog << "Followed conf file from " << FileName << " to " << Res << endl;
      Itm.Name = strcpy(FileName,Res.c_str());      
   }
   
   /* Get information about the existing file, and attempt to restore
      a backup if it does not exist */
   struct stat LExisting;
   bool EValid = false;
   if (lstat(Itm.Name,&LExisting) != 0)
   {
      // This is bad news.
      if (errno != ENOENT)
	 return _error->Errno("stat",_("Failed to stat %s"),Itm.Name);
      
      // See if we can recover the backup file
      if (Nde.end() == false)
      {
	 snprintf(Temp,sizeof(Temp),"%s.%s",Itm.Name,TempExt);
	 if (rename(Temp,Itm.Name) != 0 && errno != ENOENT)
	    return _error->Errno("rename",_("Failed to rename %s to %s"),
				 Temp,Itm.Name);
	 if (stat(Itm.Name,&LExisting) != 0)
	 {
	    if (errno != ENOENT)
	       return _error->Errno("stat",_("Failed to stat %s"),Itm.Name);
	 }	 
	 else
	    EValid = true;
      }
   }
   else
      EValid = true;
   
   /* If the file is a link we need to stat its destination, get the
      existing file modes */
   struct stat Existing = LExisting;
   if (EValid == true && S_ISLNK(Existing.st_mode))
   {
      if (stat(Itm.Name,&Existing) != 0)
      {
	 if (errno != ENOENT)
	    return _error->Errno("stat",_("Failed to stat %s"),Itm.Name);
	 Existing = LExisting;
      }      
   }
   
   // We pretend a non-existing file looks like it is a normal file
   if (EValid == false)
      Existing.st_mode = S_IFREG;
   
   /* Okay, at this point 'Existing' is the stat information for the
      real non-link file */
   
   /* The only way this can be a no-op is if a directory is being
      replaced by a directory or by a link */
   if (S_ISDIR(Existing.st_mode) != 0 && 
       (Itm.Type == Item::Directory || Itm.Type == Item::SymbolicLink))
      return true;
      
   /* Non-Directory being replaced by non-directory. We check for over
      writes here. */
   if (Nde.end() == false)
   {
      if (HandleOverwrites(Nde) == false)
	 return false;
   }
   
   /* Directory being replaced by a non-directory - this needs to see if
      the package is the owner and then see if the directory would be
      empty after the package is removed [ie no user files will be 
      erased] */
   if (S_ISDIR(Existing.st_mode) != 0)
   {
      if (CheckDirReplace(Itm.Name) == false)
	 return _error->Error(_("The directory %s is being replaced by a non-directory"),Itm.Name);
   }
   
   if (Debug == true)
      clog << "Extract " << string(Itm.Name,End) << endl;
/*   if (Count != 0)
      return _error->Error(_("Done"));*/
   
   return true;
}
									/*}}}*/
// Extract::Finished - Sequence finished, erase the temp files		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgExtract::Finished()
{
   return true;
}
									/*}}}*/
// Extract::Aborted - Sequence aborted, undo all our unpacking		/*{{{*/
// ---------------------------------------------------------------------
/* This undoes everything that was done by all calls to the DoItem method
   and restores the File Listing cache to its original form. It bases its
   actions on the flags value for each node in the cache. */
bool pkgExtract::Aborted()
{
   if (Debug == true)
      clog << "Aborted, backing out" << endl;
   
   pkgFLCache::NodeIterator Files = FLPkg.Files();
   map_ptrloc *Last = &FLPkg->Files;
   
   /* Loop over all files, restore those that have been unpacked from their
      dpkg-tmp entires */
   while (Files.end() == false)
   {
      // Locate the hash bucket for the node and locate its group head
      pkgFLCache::NodeIterator Nde(FLCache,FLCache.HashNode(Files));
      for (; Nde.end() == false && Files->File != Nde->File; Nde++);
      if (Nde.end() == true)
	 return _error->Error(_("Failed to locate node in its hash bucket"));
      
      if (snprintf(FileName,sizeof(FileName)-20,"%s/%s",
		   Nde.DirN(),Nde.File()) <= 0)
	 return _error->Error(_("The path is too long"));
      
      // Deal with diversions
      if ((Nde->Flags & pkgFLCache::Node::Diversion) != 0)
      {
	 pkgFLCache::DiverIterator Div = Nde.Diversion();
	 
	 // See if it is us and we are following it in the right direction
	 if (Div->OwnerPkg != FLPkg.Offset() && Div.DivertFrom() == Nde)
	 {
	    Nde = Div.DivertTo();
	    if (snprintf(FileName,sizeof(FileName)-20,"%s/%s",
			 Nde.DirN(),Nde.File()) <= 0)
	       return _error->Error(_("The diversion path is too long"));
	 }
      }      
      
      // Deal with overwrites+replaces
      for (; Nde.end() == false && Files->File == Nde->File; Nde++)
      {
	 if ((Nde->Flags & pkgFLCache::Node::Replaced) == 
	     pkgFLCache::Node::Replaced)
	 {
	    if (Debug == true)
	       clog << "De-replaced " << FileName << " from " << Nde.RealPackage()->Name << endl;
	    Nde->Flags &= ~pkgFLCache::Node::Replaced;
	 }	 
      }      
      
      // Undo the change in the filesystem
      if (Debug == true)
	 clog << "Backing out " << FileName;
      
      // Remove a new node
      if ((Files->Flags & pkgFLCache::Node::NewFile) ==
	 pkgFLCache::Node::NewFile)
      {
	 if (Debug == true)
	    clog << " [new node]" << endl;
	 pkgFLCache::Node *Tmp = Files;
	 Files++;
	 *Last = Tmp->NextPkg;
	 Tmp->NextPkg = 0;

	 FLCache.DropNode(Tmp - FLCache.NodeP);
      }
      else
      {
	 if (Debug == true)
	    clog << endl;
	 
	 Last = &Files->NextPkg;
	 Files++;
      }      	 
   }
   
   return true;
}
									/*}}}*/
// Extract::Fail - Extraction of a file Failed				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgExtract::Fail(Item &Itm,int Fd)
{
   return pkgDirStream::Fail(Itm,Fd);
}
									/*}}}*/
// Extract::FinishedFile - Finished a file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgExtract::FinishedFile(Item &Itm,int Fd)
{
   return pkgDirStream::FinishedFile(Itm,Fd);
}
									/*}}}*/
// Extract::HandleOverwrites - See if a replaces covers this overwrite	/*{{{*/
// ---------------------------------------------------------------------
/* Check if the file is in a package that is being replaced by this 
   package or if the file is being overwritten. Note that if the file
   is really a directory but it has been erased from the filesystem 
   this will fail with an overwrite message. This is a limitation of the
   dpkg file information format. 
 
   XX If a new package installs and another package replaces files in this
   package what should we do? */
bool pkgExtract::HandleOverwrites(pkgFLCache::NodeIterator Nde,
				  bool DiverCheck)
{
   pkgFLCache::NodeIterator TmpNde = Nde;
   unsigned long DiverOwner = 0;
   unsigned long FileGroup = Nde->File;
   const char *FirstOwner = 0;
   for (; Nde.end() == false && FileGroup == Nde->File; Nde++)
   {
      if ((Nde->Flags & pkgFLCache::Node::Diversion) != 0)
      {
	 /* Store the diversion owner if this is the forward direction
	    of the diversion */
	 if (DiverCheck == true)
	    DiverOwner = Nde.Diversion()->OwnerPkg;
	 continue;
      }

      pkgFLCache::PkgIterator FPkg(FLCache,Nde.RealPackage());	 
      if (FPkg.end() == true || FPkg == FLPkg)
	 continue;
      
      /* This tests trips when we are checking a diversion to see
         if something has already been diverted by this diversion */
      if (FPkg.Offset() == DiverOwner)
	 continue;
      FirstOwner = FPkg.Name();
      
      // Now see if this package matches one in a replace depends
      pkgCache::DepIterator Dep = Ver.DependsList();
      bool Ok = false;
      for (; Dep.end() == false; Dep++)
      {
	 if (Dep->Type != pkgCache::Dep::Replaces)
	    continue;
	 
	 // Does the replaces apply to this package?
	 if (strcmp(Dep.TargetPkg().Name(),FPkg.Name()) != 0)
	     continue;
	 
	 /* Check the version for match. I do not think CurrentVer can be
	    0 if we are here.. */
	 pkgCache::PkgIterator Pkg = Dep.TargetPkg();
	 if (Pkg->CurrentVer == 0)
	 {
	    _error->Warning(_("Overwrite package match with no version for %s"),Pkg.Name());
	    continue;
	 }

	 // Replaces is met
	 if (debVS.CheckDep(Pkg.CurrentVer().VerStr(),Dep->CompareOp,Dep.TargetVer()) == true)
	 {
	    if (Debug == true)
	       clog << "Replaced file " << Nde.DirN() << '/' << Nde.File() << " from " << Pkg.Name() << endl;
	    Nde->Flags |= pkgFLCache::Node::Replaced;
	    Ok = true;
	    break;
	 }
      }
      
      // Negative Hit
      if (Ok == false)
	 return _error->Error(_("File %s/%s overwrites the one in the package %s"),
			      Nde.DirN(),Nde.File(),FPkg.Name());
   }
   
   /* If this is a diversion we might have to recurse to process
      the other side of it */
   if ((TmpNde->Flags & pkgFLCache::Node::Diversion) != 0)
   {
      pkgFLCache::DiverIterator Div = TmpNde.Diversion();
      if (Div.DivertTo() == TmpNde)
	 return HandleOverwrites(Div.DivertFrom(),true);
   }
   
   return true;
}
									/*}}}*/
// Extract::CheckDirReplace - See if this directory can be erased	/*{{{*/
// ---------------------------------------------------------------------
/* If this directory is owned by a single package and that package is
   replacing it with something non-directoryish then dpkg allows this.
   We increase the requirement to be that the directory is non-empty after
   the package is removed */
bool pkgExtract::CheckDirReplace(string Dir,unsigned int Depth)
{
   // Looping?
   if (Depth > 40)
      return false;
   
   if (Dir[Dir.size() - 1] != '/')
      Dir += '/';
   
   DIR *D = opendir(Dir.c_str());
   if (D == 0)
      return _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());

   string File;
   for (struct dirent *Dent = readdir(D); Dent != 0; Dent = readdir(D))
   {
      // Skip some files
      if (strcmp(Dent->d_name,".") == 0 ||
	  strcmp(Dent->d_name,"..") == 0)
	 continue;
      
      // Look up the node
      File = Dir + Dent->d_name;
      pkgFLCache::NodeIterator Nde = FLCache.GetNode(File.c_str(),
						     File.c_str() + File.length(),0,false,false);

      // The file is not owned by this package
      if (Nde.end() != false || Nde.RealPackage() != FLPkg)
      {
	 closedir(D);
	 return false;
      }
      
      // See if it is a directory
      struct stat St;
      if (lstat(File.c_str(),&St) != 0)
      {
	 closedir(D);
	 return _error->Errno("lstat",_("Unable to stat %s"),File.c_str());
      }
      
      // Recurse down directories
      if (S_ISDIR(St.st_mode) != 0)
      {
	 if (CheckDirReplace(File,Depth + 1) == false)
	 {
	    closedir(D);
	    return false;
	 }
      }      
   }
   
   // No conflicts
   closedir(D);
   return true;
}
									/*}}}*/
