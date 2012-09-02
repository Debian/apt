#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/indexcopy.h>

#include <string>

#include "assert.h"

class NoCopy : public IndexCopy {
public:
   std::string ConvertToSourceList(std::string CD,std::string Path) {
      IndexCopy::ConvertToSourceList(CD, Path);
      return Path;
   }
   bool GetFile(std::string &Filename,unsigned long long &Size) { return false; }
   bool RewriteEntry(FILE *Target,std::string File) { return false; }
   const char *GetFileName() { return NULL; }
   const char *Type() { return NULL; }

};

int main(int argc, char const *argv[]) {
   NoCopy ic;
   std::string const CD("/media/cdrom/");

   char const * Releases[] = { "unstable", "wheezy-updates", NULL };
   char const * Components[] = { "main", "non-free", NULL };

   for (char const ** Release = Releases; *Release != NULL; ++Release) {
      for (char const ** Component = Components; *Component != NULL; ++Component) {
	 std::string const Path = std::string("dists/") + *Release + "/" + *Component + "/";
	 std::string const Binary = Path + "binary-";
	 std::string const A = Binary + "armel/";
	 std::string const B = Binary + "mips/";
	 std::string const C = Binary + "kfreebsd-mips/";
	 std::string const S = Path + "source/";
	 std::string const List = std::string(*Release) + " " + *Component;

	 _config->Clear("APT");
	 APT::Configuration::getArchitectures(false);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + A), A);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + B), B);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + C), C);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + S), List);

	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "mips");
	 _config->Set("APT::Architectures::", "mips");
	 APT::Configuration::getArchitectures(false);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + A), A);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + B), List);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + C), C);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + S), List);

	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "kfreebsd-mips");
	 _config->Set("APT::Architectures::", "kfreebsd-mips");
	 APT::Configuration::getArchitectures(false);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + A), A);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + B), B);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + C), List);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + S), List);

	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "armel");
	 _config->Set("APT::Architectures::", "armel");
	 APT::Configuration::getArchitectures(false);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + A), List);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + B), B);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + C), C);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + S), List);

	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "armel");
	 _config->Set("APT::Architectures::", "armel");
	 _config->Set("APT::Architectures::", "mips");
	 APT::Configuration::getArchitectures(false);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + A), List);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + B), List);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + C), C);
	 equals(ic.ConvertToSourceList("/media/cdrom/", CD + S), List);
      }
   }

   return 0;
}
