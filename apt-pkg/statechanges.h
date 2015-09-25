#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/macros.h>

#include <memory>

namespace APT
{

/** Simple wrapper class to abstract away the differences in storing different
 *  states in different places potentially in different versions.
 */
class APT_PUBLIC StateChanges
{
public:
   // getter/setter for the different states
   APT::VersionVector& Hold();
   void Hold(pkgCache::VerIterator const &Ver);
   APT::VersionVector& Unhold();
   void Unhold(pkgCache::VerIterator const &Ver);
   APT::VersionVector& Error();

   // forgets all unsaved changes
   void Discard();

   /** commit the staged changes to the database(s).
    *
    * Makes the needed calls to store the requested states.
    * After this call the state containers will hold only versions
    * for which the storing operation succeeded. Versions where the
    * storing operation failed are collected in #Error(). Note that
    * error is an upper bound as states are changed in batches so it
    * isn't always clear which version triggered the failure exactly.
    *
    * @param DiscardOutput controls if stdout/stderr should be used
    *   by subprocesses for (detailed) error reporting if needed.
    * @return \b false if storing failed, true otherwise.
    *   Note that some states might be applied even if the whole operation failed.
    */
   bool Save(bool const DiscardOutput = false);

   StateChanges();
   StateChanges(StateChanges&&);
   StateChanges& operator=(StateChanges&&);
   ~StateChanges();

private:
   class APT_HIDDEN Private;
   std::unique_ptr<Private> d;
};

}
