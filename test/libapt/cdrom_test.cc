#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/cdrom.h>
#include <apt-pkg/cdromutl.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <string.h>
#include <vector>

#include <gtest/gtest.h>

#include "file-helpers.h"

class Cdrom : public pkgCdrom {
public:
   std::vector<std::string> ReduceSourcelist(std::string CD,std::vector<std::string> List) {
      pkgCdrom::ReduceSourcelist(CD, List);
      return List;
   }
};

TEST(CDROMTest,ReduceSourcelist)
{
   Cdrom cd;
   std::vector<std::string> List;
   std::string CD("/media/cdrom/");

   std::vector<std::string> R = cd.ReduceSourcelist(CD, List);
   EXPECT_TRUE(R.empty());

   List.push_back(" wheezy main");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(1, R.size());
   EXPECT_EQ(" wheezy main", R[0]);

   List.push_back(" wheezy main");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(1, R.size());
   EXPECT_EQ(" wheezy main", R[0]);

   List.push_back(" wheezy contrib");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(1, R.size());
   EXPECT_EQ(" wheezy contrib main", R[0]);

   List.push_back(" wheezy-update contrib");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(2, R.size());
   EXPECT_EQ(" wheezy contrib main", R[0]);
   EXPECT_EQ(" wheezy-update contrib", R[1]);

   List.push_back(" wheezy-update contrib");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(2, R.size());
   EXPECT_EQ(" wheezy contrib main", R[0]);
   EXPECT_EQ(" wheezy-update contrib", R[1]);

   List.push_back(" wheezy-update non-free");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(2, R.size());
   EXPECT_EQ(" wheezy contrib main", R[0]);
   EXPECT_EQ(" wheezy-update contrib non-free", R[1]);

   List.push_back(" wheezy-update main");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(2, R.size());
   EXPECT_EQ(" wheezy contrib main", R[0]);
   EXPECT_EQ(" wheezy-update contrib main non-free", R[1]);

   List.push_back(" wheezy non-free");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(2, R.size());
   EXPECT_EQ(" wheezy contrib main non-free", R[0]);
   EXPECT_EQ(" wheezy-update contrib main non-free", R[1]);

   List.push_back(" sid main");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(3, R.size());
   EXPECT_EQ(" sid main", R[0]);
   EXPECT_EQ(" wheezy contrib main non-free", R[1]);
   EXPECT_EQ(" wheezy-update contrib main non-free", R[2]);

   List.push_back(" sid main-reduce");
   R = cd.ReduceSourcelist(CD, List);
   ASSERT_EQ(3, R.size());
   EXPECT_EQ(" sid main main-reduce", R[0]);
   EXPECT_EQ(" wheezy contrib main non-free", R[1]);
   EXPECT_EQ(" wheezy-update contrib main non-free", R[2]);
}
TEST(CDROMTest, FindMountPointForDevice)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("mountpoints", fd, &tempfile,
	 "rootfs / rootfs rw 0 0\n"
	 "sysfs /sys sysfs rw,nosuid,nodev,noexec,relatime 0 0\n"
	 "sysfs0 /sys0 sysfs rw,nosuid,nodev,noexec,relatime 0 0\n"
	 "/dev/disk/by-uuid/fadcbc52-6284-4874-aaaa-dcee1f05fe21 / ext4 rw,relatime,errors=remount-ro,data=ordered 0 0\n"
	 "/dev/sda1 /boot/efi vfat rw,nosuid,nodev,noexec,relatime,fmask=0000,dmask=0000,allow_utime=0022,codepage=437,iocharset=utf8,shortname=lower,quiet,utf8,errors=remount-ro,rw,nosuid,nodev,noexec,relatime,fmask=0000,dmask=0000,allow_utime=0022,codepage=437,iocharset=utf8,shortname=lower,quiet,utf8,errors=remount-ro,rw,nosuid,nodev,noexec,relatime,fmask=0000,dmask=0000,allow_utime=0022,codepage=437,iocharset=utf8,shortname=lower,quiet,utf8,errors=remount-ro,rw,nosuid,nodev,noexec,relatime,fmask=0000,dmask=0000,allow_utime=0022,codepage=437,iocharset=utf8,shortname=lower,quiet,utf8,errors=remount-ro 0 0\n"
	 "tmpfs /tmp tmpfs rw,nosuid,nodev,relatime 0 0\n");
   _config->Set("Dir::state::Mountpoints", tempfile);

   EXPECT_EQ("/", FindMountPointForDevice("rootfs"));
   EXPECT_EQ("/", FindMountPointForDevice("/dev/disk/by-uuid/fadcbc52-6284-4874-aaaa-dcee1f05fe21"));
   EXPECT_EQ("/sys", FindMountPointForDevice("sysfs"));
   EXPECT_EQ("/sys0", FindMountPointForDevice("sysfs0"));
   EXPECT_EQ("/boot/efi", FindMountPointForDevice("/dev/sda1"));
   EXPECT_EQ("/tmp", FindMountPointForDevice("tmpfs"));

   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
}
