#include <config.h>

#include <apt-pkg/cdromutl.h>
#include <apt-pkg/configuration.h>

#include <string>
#include <vector>

#include "assert.h"

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		std::cout << "One parameter expected - given " << argc << std::endl;
		return 100;
	}

	_config->Set("Dir::state::Mountpoints", argv[1]);
	equals("/", FindMountPointForDevice("rootfs"));
	equals("/", FindMountPointForDevice("/dev/disk/by-uuid/fadcbc52-6284-4874-aaaa-dcee1f05fe21"));
	equals("/sys", FindMountPointForDevice("sysfs"));
	equals("/sys0", FindMountPointForDevice("sysfs0"));
	equals("/boot/efi", FindMountPointForDevice("/dev/sda1"));
	equals("/tmp", FindMountPointForDevice("tmpfs"));

	return 0;
}
