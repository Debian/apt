// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   ByHash 
   
   ByHash helper functions
   
   ##################################################################### */
									/*}}}*/
#ifndef BYHASH_H
#define BYHASH_H

#include <string>

class HashString;

// Delete all files in "dir" except for the number specified in "KeepFiles"
// that are the most recent ones
void DeleteAllButMostRecent(std::string dir, int KeepFiles);

// takes a regular input filename
std::string GenByHashFilename(std::string Input, HashString const &h);

#endif
