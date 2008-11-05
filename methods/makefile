# -*- make -*-
BASE=..
SUBDIR=methods

# Bring in the default rules
include ../buildlib/defaults.mak
BIN := $(BIN)/methods

# FIXME..
LIB_APT_PKG_MAJOR = 4.6
APT_DOMAIN := libapt-pkg$(LIB_APT_PKG_MAJOR)

# The file method
PROGRAM=file
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = file.cc
include $(PROGRAM_H)

# The copy method
PROGRAM=copy
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = copy.cc
include $(PROGRAM_H)

# The gzip method
PROGRAM=gzip
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = gzip.cc
include $(PROGRAM_H)

# The gpgv method
PROGRAM=gpgv
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = gpgv.cc
include $(PROGRAM_H)

# The cdrom method
PROGRAM=cdrom
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = cdrom.cc
include $(PROGRAM_H)

# The http method
PROGRAM=http
SLIBS = -lapt-pkg $(SOCKETLIBS) $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = http.cc rfc2553emu.cc connect.cc
include $(PROGRAM_H)

# The https method
PROGRAM=https
SLIBS = -lapt-pkg -lcurl $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = https.cc
include $(PROGRAM_H)

# The ftp method
PROGRAM=ftp
SLIBS = -lapt-pkg $(SOCKETLIBS) $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = ftp.cc rfc2553emu.cc connect.cc
include $(PROGRAM_H)

# The rred method
PROGRAM=rred
SLIBS = -lapt-pkg $(SOCKETLIBS) $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = rred.cc
include $(PROGRAM_H)

# The rsh method
PROGRAM=rsh
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = rsh.cc
include $(PROGRAM_H)

# SSH and bzip2 method symlink
binary: $(BIN)/ssh $(BIN)/bzip2 $(BIN)/lzma
veryclean: clean-$(BIN)/ssh clean-$(BIN)/bzip2 clean-$(BIN)/lzma
$(BIN)/ssh:
	echo "Installing ssh method link"
	ln -fs rsh $(BIN)/ssh
clean-$(BIN)/ssh:
	-rm $(BIN)/ssh

$(BIN)/bzip2:
	echo "Installing bzip2 method link"
	ln -fs gzip $(BIN)/bzip2
$(BIN)/lzma:
	echo "Installing lzma method link"
	ln -fs gzip $(BIN)/lzma
clean-$(BIN)/bzip2:
	-rm $(BIN)/bzip2
clean-$(BIN)/lzma:
	-rm $(BIN)/lzma
