#!/usr/bin/python

import sys
import os
import glob
import os.path
import shutil
import time
from subprocess import call, PIPE

import unittest

stdout = os.open("/dev/null",0) #sys.stdout
stderr = os.open("/dev/null",0) # sys.stderr

apt_args = [] 
#apt_args = ["-o","Debug::pkgAcquire::Auth=true"]

class testAptAuthenticationReliability(unittest.TestCase):
    """
    test if the spec https://wiki.ubuntu.com/AptAuthenticationReliability 
    is properly implemented
    """
    #apt = "../bin/apt-get"
    apt = "apt-get"

    def setUp(self):
        if os.path.exists("/tmp/autFailure"):
            os.unlink("/tmp/authFailure");
        if os.path.exists("/tmp/autFailure2"):
            os.unlink("/tmp/authFailure2");
    def testRepositorySigFailure(self):
        """
        test if a repository that used to be authenticated and fails on
        apt-get update refuses to update and uses the old state
        """
        # copy valid signatures into lists (those are ok, even
        # if the name is "-broken-" ...
        for f in glob.glob("./authReliability/lists/*"):
            shutil.copy(f,"/var/lib/apt/lists")
            # ensure we do *not* get a I-M-S hit
            os.utime("/var/lib/apt/lists/%s" % os.path.basename(f), (0,0))
        res = call([self.apt,
                    "update",
                    "-o","Dir::Etc::sourcelist=./authReliability/sources.list.failure", 
                    "-o",'APT::Update::Auth-Failure::=touch /tmp/authFailure',
                   ] + apt_args,
                   stdout=stdout, stderr=stderr)
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-broken_Release.gpg"),
                     "The gpg file disappeared, this should not happen")
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-broken_Packages"),
                     "The Packages file disappeared, this should not happen")
        self.assert_(os.path.exists("/tmp/authFailure"),
                     "The APT::Update::Auth-Failure script did not run (1)")
        # the same with i-m-s hit this time
        for f in glob.glob("./authReliability/lists/*"):
            shutil.copy(f,"/var/lib/apt/lists")
            os.utime("/var/lib/apt/lists/%s" % os.path.basename(f), (time.time(),time.time()))
        res = call([self.apt,
                    "update",
                    "-o","Dir::Etc::sourcelist=./authReliability/sources.list.failure",
                    "-o",'APT::Update::Auth-Failure::=touch /tmp/authFailure2',
                   ] + apt_args,
                   stdout=stdout, stderr=stderr)
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-broken_Release.gpg"),
                     "The gpg file disappeared, this should not happen")
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-broken_Packages"),
                     "The Packages file disappeared, this should not happen")
        self.assert_(os.path.exists("/tmp/authFailure2"),
                     "The APT::Update::Auth-Failure script did not run (2)")
    def testRepositorySigGood(self):
        """
        test that a regular repository with good data stays good
        """
        res = call([self.apt,
                    "update",
                    "-o","Dir::Etc::sourcelist=./authReliability/sources.list.good"
                   ] + apt_args,
                   stdout=stdout, stderr=stderr)
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-ok_Release.gpg"),
                     "The gpg file disappeared after a regular download, this should not happen")
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-ok_Packages"),
                     "The Packages file disappeared, this should not happen")
        # test good is still good after non I-M-S hit and a previous files in lists/
        for f in glob.glob("./authReliability/lists/*"):
            shutil.copy(f,"/var/lib/apt/lists")
            # ensure we do *not* get a I-M-S hit
            os.utime("/var/lib/apt/lists/%s" % os.path.basename(f), (0,0))
        res = call([self.apt,
                    "update",
                    "-o","Dir::Etc::sourcelist=./authReliability/sources.list.good"
                   ] + apt_args,
                   stdout=stdout, stderr=stderr)
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-ok_Release.gpg"),
                     "The gpg file disappeared after a I-M-S hit, this should not happen")
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-ok_Packages"),
                     "The Packages file disappeared, this should not happen")
        # test good is still good after I-M-S hit
        for f in glob.glob("./authReliability/lists/*"):
            shutil.copy(f,"/var/lib/apt/lists")
            # ensure we do get a I-M-S hit
            os.utime("/var/lib/apt/lists/%s" % os.path.basename(f), (time.time(),time.time()))
        res = call([self.apt,
                    "update",
                    "-o","Dir::Etc::sourcelist=./authReliability/sources.list.good"
                   ] + apt_args,
                   stdout=stdout, stderr=stderr)
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-ok_Release.gpg"),
                     "The gpg file disappeared, this should not happen")
        self.assert_(os.path.exists("/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_auth-test-suit_gpg-package-ok_Packages"),
                     "The Packages file disappeared, this should not happen")


class testAuthentication(unittest.TestCase):
    """
    test if the authentication is working, the repository
    of the test-data can be found here:
    bzr get http://people.ubuntu.com/~mvo/bzr/apt/apt-auth-test-suit/
    """
    
    # some class wide data
    apt = "apt-get"
    pkg = "libglib2.0-data"
    pkgver = "2.13.6-1ubuntu1"
    pkgpath = "/var/cache/apt/archives/libglib2.0-data_2.13.6-1ubuntu1_all.deb"

    def setUp(self):
        for f in glob.glob("testkeys/*,key"):
            call(["apt-key", "add", f], stdout=stdout, stderr=stderr)

    def _cleanup(self):
        " make sure we get new lists and no i-m-s "
        call(["rm","-f", "/var/lib/apt/lists/*"])
        if os.path.exists(self.pkgpath):
            os.unlink(self.pkgpath)

    def _expectedRes(self, resultstr):
        if resultstr == 'ok':
            return 0
        elif resultstr == 'broken':
            return 100
        

    def testPackages(self):
        for f in glob.glob("testsources.list/sources.list*package*"):
            self._cleanup()
            (prefix, testtype, result) = f.split("-")
            expected_res = self._expectedRes(result)
            # update first
            call([self.apt,"update",
                  "-o","Dir::Etc::sourcelist=./%s" % f]+apt_args,
                 stdout=stdout, stderr=stderr)
            # then get the pkg
            cmd = ["install", "-y", "-d", "--reinstall",
                   "%s=%s" % (self.pkg, self.pkgver),
                   "-o","Dir::state::Status=./fake-status"]
            res = call([self.apt, "-o","Dir::Etc::sourcelist=./%s" % f]+cmd+apt_args,
                       stdout=stdout, stderr=stderr)
            self.assert_(res == expected_res,
                         "test '%s' failed (got %s expected %s" % (f,res,expected_res))
            

    def testGPG(self):
        for f in glob.glob("testsources.list/sources.list*gpg*"):
            self._cleanup()
            (prefix, testtype, result) = f.split("-")
            expected_res = self._expectedRes(result)
            # update first
            call([self.apt,"update",
                  "-o","Dir::Etc::sourcelist=./%s" % f]+apt_args,
                 stdout=stdout, stderr=stderr)
            cmd = ["install", "-y", "-d", "--reinstall",
                   "%s=%s" % (self.pkg, self.pkgver),
                   "-o","Dir::state::Status=./fake-status"]
            res = call([self.apt, "-o","Dir::Etc::sourcelist=./%s" % f]+
                       cmd+apt_args,
                       stdout=stdout, stderr=stderr)
            self.assert_(res == expected_res,
                         "test '%s' failed (got %s expected %s" % (f,res,expected_res))

    def testRelease(self):
        for f in glob.glob("testsources.list/sources.list*release*"):
            self._cleanup()
            (prefix, testtype, result) = f.split("-")
            expected_res = self._expectedRes(result)
            cmd = ["update"]
            res = call([self.apt,"-o","Dir::Etc::sourcelist=./%s" % f]+cmd+apt_args,
                       stdout=stdout, stderr=stderr)
            self.assert_(res == expected_res,
                         "test '%s' failed (got %s expected %s" % (f,res,expected_res))
            if expected_res == 0:
                self.assert_(len(glob.glob("/var/lib/apt/lists/partial/*")) == 0,
                             "partial/ dir has leftover files: %s" % glob.glob("/var/lib/apt/lists/partial/*"))


class testLocalRepositories(unittest.TestCase):
    " test local repository regressions "

    repo_dir = "local-repo"
    apt = "apt-get"
    pkg = "gdebi-test4"

    def setUp(self):
        self.repo = os.path.abspath(os.path.join(os.getcwd(), self.repo_dir))
        self.sources = os.path.join(self.repo, "sources.list")
        s = open(self.sources,"w")
        s.write("deb file://%s/ /\n" % self.repo)
        s.close()

    def testLocalRepoAuth(self):
        # two times to get at least one i-m-s hit
        for i in range(2):
            self.assert_(os.path.exists(self.sources))
            cmd = [self.apt,"update","-o", "Dir::Etc::sourcelist=%s" % self.sources]+apt_args
            res = call(cmd, stdout=stdout, stderr=stderr)
            self.assertEqual(res, 0, "local repo test failed")
            self.assert_(os.path.exists(os.path.join(self.repo,"Packages.gz")),
                         "Packages.gz vanished from local repo")

    def testInstallFromLocalRepo(self):
        apt = [self.apt,"-o", "Dir::Etc::sourcelist=%s"% self.sources]+apt_args
        cmd = apt+["update"]
        res = call(cmd, stdout=stdout, stderr=stderr)
        self.assertEqual(res, 0)
        res = call(apt+["-y","install","--reinstall",self.pkg],
                   stdout=stdout, stderr=stderr)
        self.assert_(res == 0,
                     "installing %s failed (got %s)" % (self.pkg, res))
        res = call(apt+["-y","remove",self.pkg],
                   stdout=stdout, stderr=stderr)
        self.assert_(res == 0,
                     "removing %s failed (got %s)" % (self.pkg, res))

    def testPythonAptInLocalRepo(self):
        import apt, apt_pkg
        apt_pkg.Config.Set("Dir::Etc::sourcelist",self.sources)
        cache = apt.Cache()
        cache.update()
        pkg = cache["apt"]
        self.assert_(pkg.name == 'apt')
        


if __name__ == "__main__":
    print "Runing simple testsuit on current apt-get and libapt"
    if len(sys.argv) > 1 and sys.argv[1] == "-v":
        stdout = sys.stdout
        stderr = sys.stderr
    
    # run only one for now
    #unittest.main(defaultTest="testAptAuthenticationReliability")
    unittest.main()
