set(languages_excluded
    he  ## disabled by translator request #313283
)

apt_add_translation_domain(
    DOMAIN libapt-pkg${APT_PKG_MAJOR}
    TARGETS apt-pkg
    EXCLUDE_LANGUAGES ${languages_excluded}
)

apt_add_translation_domain(
    DOMAIN apt
    TARGETS apt apt-cache apt-get apt-config apt-cdrom apt-helper apt-mark
            apt-private
            # Methods
            file copy store gpgv cdrom http https ftp rred rsh mirror
    SCRIPTS ../dselect/install ../dselect/update
    EXCLUDE_LANGUAGES ${languages_excluded}
)

apt_add_translation_domain(
    DOMAIN apt-utils
    TARGETS apt-sortpkgs
            apt-extracttemplates
            apt-internal-solver
            apt-dump-solver
            apt-internal-planner
            apt-ftparchive
    EXCLUDE_LANGUAGES ${languages_excluded}
)

apt_add_translation_domain(
    DOMAIN libapt-inst${APT_INST_MAJOR}
    TARGETS apt-inst
    EXCLUDE_LANGUAGES ${languages_excluded}
)

apt_add_update_po(
    TEMPLATE
        apt-all
    DOMAINS
        libapt-pkg${APT_PKG_MAJOR}
        libapt-inst${APT_INST_MAJOR}
        apt
        apt-utils
    EXCLUDE_LANGUAGES
        ${languages_excluded}
)

apt_add_po_statistics("${languages_excluded}")
