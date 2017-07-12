#include <apt-pkg/cacheset.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

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
#define APT_GETTERSETTER(Name) \
   APT::VersionVector& Name(); \
   void Name(pkgCache::VerIterator const &Ver)
   APT_GETTERSETTER(Hold);
   APT_GETTERSETTER(Unhold);
   APT_GETTERSETTER(Install);
   APT_GETTERSETTER(Remove);
   APT_GETTERSETTER(Purge);
   APT::VersionVector& Error();
#undef APT_GETTERSETTER

   // operate on all containers at once
   void clear();
   bool empty() const;

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
