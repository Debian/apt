#!/usr/bin/python

import sys
import os
import glob
import os.path
from subprocess import call, PIPE

import unittest

stdout = os.open("/dev/null",0) #sys.stdout
stderr = os.open("/dev/null",0) # sys.stderr

apt_args = []  # ["-o","Debug::pkgAcquire::Auth=true"]


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
    unittest.main()


