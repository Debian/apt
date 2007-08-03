#!/usr/bin/python

import sys
import os
import glob
import os.path
from subprocess import call, PIPE

import unittest

stdout = os.open("/dev/null",0) #sys.stdout
stderr = os.open("/dev/null",0) # sys.stderr

class testAuthentication(unittest.TestCase):

    # some class wide data
    apt = "apt-get"
    args = []  # ["-q", "-q", "-o","Debug::pkgAcquire::Auth=true"]
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
                  "-o","Dir::Etc::sourcelist=./%s" % f]+self.args,
                 stdout=stdout, stderr=stderr)
            # then get the pkg
            cmd = ["install", "-y", "-d", "--reinstall",
                   "%s=%s" % (self.pkg, self.pkgver),
                   "-o","Dir::state::Status=./fake-status"]
            res = call([self.apt, "-o","Dir::Etc::sourcelist=./%s" % f]+cmd+self.args,
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
                  "-o","Dir::Etc::sourcelist=./%s" % f]+self.args,
                 stdout=stdout, stderr=stderr)
            # then get the pkg
            cmd = ["install", "-y", "-d", "--reinstall",
                   "%s=%s" % (self.pkg, self.pkgver),
                   "-o","Dir::state::Status=./fake-status"]
            res = call([self.apt, "-o","Dir::Etc::sourcelist=./%s" % f]+
                       cmd+self.args,
                       stdout=stdout, stderr=stderr)
            self.assert_(res == expected_res,
                         "test '%s' failed (got %s expected %s" % (f,res,expected_res))

    def testRelease(self):
        for f in glob.glob("testsources.list/sources.list*release*"):
            self._cleanup()
            (prefix, testtype, result) = f.split("-")
            expected_res = self._expectedRes(result)
            cmd = ["update"]
            res = call([self.apt,"-o","Dir::Etc::sourcelist=./%s" % f]+cmd+self.args,
                       stdout=stdout, stderr=stderr)
            self.assert_(res == expected_res,
                         "test '%s' failed (got %s expected %s" % (f,res,expected_res))


class testPythonApt(unittest.TestCase):
    " test if python-apt is still working and if we not accidently broke the ABI "
    
    def testPythonApt(self):
        import apt
        cache = apt.Cache()
        cache.update()
        pkg = cache["apt"]
        self.assert_(pkg.name == 'apt')

class testAptInstall(unittest.TestCase):
    " test if installing still works "

    apt = "apt-get"
    pkg = "coreutils"

    def testInstall(self):
        res = call([self.apt,"-y","install","--reinstall",self.pkg],
                   stdout=stdout, stderr=stderr)
        self.assert_(res == 0,
                     "installing %s failed (got %s)" % (self.pkg, res))

if __name__ == "__main__":
    print "Runing simple testsuit on current apt-get and libapt"
    if len(sys.argv) > 1 and sys.argv[1] == "-v":
        stdout = sys.stdout
        stderr = sys.stderr
    unittest.main()


