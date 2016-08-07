apt_add_translation_domain(
    DOMAIN libapt-pkg${APT_PKG_MAJOR}
    TARGETS apt-pkg
)

apt_add_translation_domain(
    DOMAIN apt
    TARGETS apt apt-cache apt-get apt-config apt-cdrom apt-helper apt-mark
            apt-private
            # Methods
            file copy store gpgv cdrom http https ftp rred rsh mirror
    SCRIPTS ../dselect/install ../dselect/update
)

apt_add_translation_domain(
    DOMAIN apt-utils
    TARGETS apt-sortpkgs
            apt-extracttemplates
            apt-internal-solver
            apt-dump-solver
            apt-internal-planner
            apt-ftparchive
)

apt_add_translation_domain(
    DOMAIN libapt-inst${APT_INST_MAJOR}
    TARGETS apt-inst
)
