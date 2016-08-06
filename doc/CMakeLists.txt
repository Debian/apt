include(Documentation)

file(GLOB_RECURSE debiandoc-apt guide*.dbk offline*.dbk)
file(GLOB_RECURSE debiandoc-libapt cache*.dbk design*.dbk dpkg-tech*.dbk
                                   files*.dbk method*.dbk)


set(manpages apt.8 apt-cache.8 apt-get.8 apt-cdrom.8 apt.conf.5 sources.list.5
             apt-config.8 apt_preferences.5 apt-sortpkgs.1 apt-ftparchive.1
             apt-extracttemplates.1 apt-key.8 apt-secure.8 apt-mark.8)

if (WITH_DOC)
add_docbook(debiandoc-apt "${debiandoc-apt}" share/doc/apt-doc)
add_docbook(debiandoc-libapt "${debiandoc-libapt}" share/doc/libapt-pkg-doc)
endif()

# Build the manpages, and add translations (ja only for now, others broken)
add_xml_manpages(doc-man "${manpages}" "de;es;fr;it;ja;nl;pl;pt_BR;pt" "apt.ent;apt-verbatim.ent")


add_subdirectory(examples)
