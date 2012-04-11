# -*- make -*-
BASE=..
SUBDIR=methods

# Bring in the default rules
include ../buildlib/defaults.mak
BIN := $(BIN)/methods

include ../buildlib/libversion.mak
APT_DOMAIN := apt

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
SLIBS = -lapt-pkg -ldl $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = cdrom.cc
include $(PROGRAM_H)

# The http method
PROGRAM=http
SLIBS = -lapt-pkg $(SOCKETLIBS) $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = http.cc http_main.cc rfc2553emu.cc connect.cc
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

# The mirror method
PROGRAM=mirror
SLIBS = -lapt-pkg $(SOCKETLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = mirror.cc http.cc rfc2553emu.cc connect.cc
include $(PROGRAM_H)

# SSH method symlink
binary: $(BIN)/ssh
veryclean: clean-$(BIN)/ssh

$(BIN)/ssh:
	echo "Installing ssh method link"
	ln -fs rsh $(BIN)/ssh
clean-$(BIN)/ssh:
	-rm $(BIN)/ssh

# create links for all other compressors
COMPRESSORS=bzip2 lzma xz

binary: $(addprefix $(BIN)/,$(COMPRESSORS))
veryclean: $(addprefix clean-$(BIN)/,$(COMPRESSORS))

$(addprefix $(BIN)/,$(COMPRESSORS)): $(BIN)/gzip
	echo "Installing $(notdir $@) method link"
	ln -fs gzip $@

$(addprefix clean-$(BIN)/,$(COMPRESSORS)):
	-rm $(BIN)/$(notdir $@)
