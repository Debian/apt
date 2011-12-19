#include <apt-pkg/cdrom.h>
#include <apt-pkg/error.h>

#include <algorithm>
#include <string>
#include <vector>

#include "assert.h"

class Cdrom : public pkgCdrom {
	public:
	bool FindPackages(std::string const &CD,
			  std::vector<std::string> &List,
			  std::vector<std::string> &SList,
			  std::vector<std::string> &SigList,
			  std::vector<std::string> &TransList,
			  std::string &InfoDir) {
		bool const result = pkgCdrom::FindPackages(CD, List, SList, SigList, TransList, InfoDir, NULL, 0);
		std::sort(List.begin(), List.end());
		std::sort(SList.begin(), SList.end());
		std::sort(SigList.begin(), SigList.end());
		std::sort(TransList.begin(), TransList.end());
		return result;
	}

	bool DropRepeats(std::vector<std::string> &List, char const *Name) {
		return pkgCdrom::DropRepeats(List, Name);
	}
};

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		std::cout << "One parameter expected - given " << argc << std::endl;
		return 100;
	}

	Cdrom cd;
	std::vector<std::string> Packages, Sources, Signatur, Translation;
	std::string InfoDir;
	std::string path = argv[1];
	equals(true, cd.FindPackages(path, Packages, Sources, Signatur, Translation, InfoDir));
	equals(4, Packages.size());
	equals(path + "/dists/sid/main/binary-i386/", Packages[0]);
	equals(path + "/dists/stable/contrib/binary-amd64/", Packages[1]);
	equals(path + "/dists/stable/main/binary-i386/", Packages[2]);
	equals(path + "/dists/unstable/main/binary-i386/", Packages[3]);
	equals(3, Sources.size());
	equals(path + "/dists/sid/main/source/", Sources[0]);
	equals(path + "/dists/stable/main/source/", Sources[1]);
	equals(path + "/dists/unstable/main/source/", Sources[2]);
	equals(3, Signatur.size());
	equals(path + "/dists/sid/", Signatur[0]);
	equals(path + "/dists/stable/", Signatur[1]);
	equals(path + "/dists/unstable/", Signatur[2]);
	equals(4, Translation.size());
	equals(path + "/dists/sid/main/i18n/Translation-de", Translation[0]);
	equals(path + "/dists/sid/main/i18n/Translation-en", Translation[1]);
	equals(path + "/dists/unstable/main/i18n/Translation-de", Translation[2]);
	equals(path + "/dists/unstable/main/i18n/Translation-en", Translation[3]);
	equals(path + "/.disk/", InfoDir);

	cd.DropRepeats(Packages, "Packages");
	cd.DropRepeats(Sources, "Sources");
	_error->PushToStack();
	cd.DropRepeats(Signatur, "InRelease");
	cd.DropRepeats(Signatur, "Release.gpg");
	_error->RevertToStack();
	_error->DumpErrors();
	cd.DropRepeats(Translation, "");

	equals(3, Packages.size());
	equals(path + "/dists/stable/contrib/binary-amd64/", Packages[0]);
	equals(path + "/dists/stable/main/binary-i386/", Packages[1]);
	equals(path + "/dists/unstable/main/binary-i386/", Packages[2]);
	equals(2, Sources.size());
	equals(path + "/dists/stable/main/source/", Sources[0]);
	equals(path + "/dists/unstable/main/source/", Sources[1]);
	equals(2, Signatur.size());
	equals(path + "/dists/stable/", Signatur[0]);
	equals(path + "/dists/unstable/", Signatur[1]);
	equals(2, Translation.size());
	equals(path + "/dists/unstable/main/i18n/Translation-de", Translation[0]);
	equals(path + "/dists/unstable/main/i18n/Translation-en", Translation[1]);

	return 0;
}
