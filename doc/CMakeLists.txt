include(Documentation)

file(GLOB_RECURSE debiandoc-apt guide*.sgml offline*.sgml)
file(GLOB_RECURSE debiandoc-libapt cache*.sgml design*.sgml dpkg-tech*.sgml 
                                   files*.sgml method*.sgml)


set(manpages apt-cache.8 apt-get.8 apt-cdrom.8 apt.conf.5 sources.list.5
             apt-config.8 apt_preferences.5 apt-sortpkgs.1 apt-ftparchive.1
             apt-extracttemplates.1 apt-key.8 apt-secure.8 apt-mark.8)
	 
add_debiandoc(debiandoc-apt "${debiandoc-apt}" share/doc/apt-doc)
add_debiandoc(debiandoc-libapt "${debiandoc-libapt}" share/doc/libapt-pkg-doc)


# Build the manpages, and add translations (ja only for now, others broken)
add_xml_manpages(doc-man "${manpages}" "ja" "apt.ent")
