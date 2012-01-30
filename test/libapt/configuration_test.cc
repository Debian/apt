#include <apt-pkg/configuration.h>

#include <string>
#include <vector>

#include "assert.h"

int main(int argc,const char *argv[]) {
	Configuration Cnf;
	std::vector<std::string> fds;

	Cnf.Set("APT::Keep-Fds::",28);
	Cnf.Set("APT::Keep-Fds::",17);
	Cnf.Set("APT::Keep-Fds::2",47);
	Cnf.Set("APT::Keep-Fds::","broken");
	fds = Cnf.FindVector("APT::Keep-Fds");
	equals(fds[0], "28");
	equals(fds[1], "17");
	equals(fds[2], "47");
	equals(fds[3], "broken");
	equals(fds.size(), 4);
	equals(Cnf.Exists("APT::Keep-Fds::2"), true);
	equals(Cnf.Find("APT::Keep-Fds::2"), "47");
	equals(Cnf.FindI("APT::Keep-Fds::2"), 47);
	equals(Cnf.Exists("APT::Keep-Fds::3"), false);
	equals(Cnf.Find("APT::Keep-Fds::3"), "");
	equals(Cnf.FindI("APT::Keep-Fds::3", 56), 56);
	equals(Cnf.Find("APT::Keep-Fds::3", "not-set"), "not-set");

	Cnf.Clear("APT::Keep-Fds::2");
	fds = Cnf.FindVector("APT::Keep-Fds");
	equals(fds[0], "28");
	equals(fds[1], "17");
	equals(fds[2], "");
	equals(fds[3], "broken");
	equals(fds.size(), 4);
	equals(Cnf.Exists("APT::Keep-Fds::2"), true);

	Cnf.Clear("APT::Keep-Fds",28);
	fds = Cnf.FindVector("APT::Keep-Fds");
	equals(fds[0], "17");
	equals(fds[1], "");
	equals(fds[2], "broken");
	equals(fds.size(), 3);

	Cnf.Clear("APT::Keep-Fds","");
	equals(Cnf.Exists("APT::Keep-Fds::2"), false);

	Cnf.Clear("APT::Keep-Fds",17);
	Cnf.Clear("APT::Keep-Fds","broken");
	fds = Cnf.FindVector("APT::Keep-Fds");
	equals(fds.empty(), true);

	Cnf.Set("APT::Keep-Fds::",21);
	Cnf.Set("APT::Keep-Fds::",42);
	fds = Cnf.FindVector("APT::Keep-Fds");
	equals(fds[0], "21");
	equals(fds[1], "42");
	equals(fds.size(), 2);

	Cnf.Clear("APT::Keep-Fds");
	fds = Cnf.FindVector("APT::Keep-Fds");
	equals(fds.empty(), true);

	Cnf.CndSet("APT::Version", 42);
	Cnf.CndSet("APT::Version", "66");
	equals(Cnf.Find("APT::Version"), "42");
	equals(Cnf.FindI("APT::Version"), 42);
	equals(Cnf.Find("APT::Version", "33"), "42");
	equals(Cnf.FindI("APT::Version", 33), 42);
	equals(Cnf.Find("APT2::Version", "33"), "33");
	equals(Cnf.FindI("APT2::Version", 33), 33);

	equals(Cnf.FindFile("Dir::State"), "");
	equals(Cnf.FindFile("Dir::Aptitude::State"), "");
	Cnf.Set("Dir", "/srv/sid");
	equals(Cnf.FindFile("Dir::State"), "");
	Cnf.Set("Dir::State", "var/lib/apt");
	Cnf.Set("Dir::Aptitude::State", "var/lib/aptitude");
	equals(Cnf.FindFile("Dir::State"), "/srv/sid/var/lib/apt");
	equals(Cnf.FindFile("Dir::Aptitude::State"), "/srv/sid/var/lib/aptitude");

	//FIXME: Test for configuration file parsing;
	// currently only integration/ tests test them implicitly

	return 0;
}
