apt_add_translation_domain(libapt-pkg${APT_PKG_MAJOR} apt-pkg)

apt_add_translation_domain(apt              # Domain
                           apt apt-cache apt-get apt-config apt-cdrom
                           apt-helper apt-mark
                           apt-private
                           # Methods
                           file copy store gpgv cdrom http https ftp rred rsh
                           mirror
)


apt_add_translation_domain(apt-utils        # Domain
                           apt-sortpkgs
                           apt-extracttemplates
                           apt-internal-solver
                           apt-dump-solver
                           apt-internal-planner
                           apt-ftparchive)

# Add the translations for libapt-inst, and set the translation domain.
apt_add_translation_domain(libapt-inst${APT_INST_MAJOR} apt-inst)
