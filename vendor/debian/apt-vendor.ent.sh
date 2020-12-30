#!/bin/sh
set -e

BASEDIR="$(readlink -f "$(dirname "$0")")"

cat <<EOF
<!-- details about the keys used by the distribution -->
<!ENTITY keyring-distro "Debian">
<!ENTITY keyring-package "<package>debian-archive-keyring</package>">
<!ENTITY keyring-filename "">
<!ENTITY keyring-removed-filename "">
<!ENTITY keyring-master-filename "">
<!ENTITY keyring-uri "">
<!ENTITY vendor-codename "&debian-stable-codename;">

EOF

if dpkg --compare-versions "$("${BASEDIR}/../getinfo" 'debian-stable-version')" '>=' '11'; then
	echo '<!ENTITY vendor-security-suite "&vendor-codename;-security">'
else
	echo '<!ENTITY vendor-security-suite "&vendor-codename;/updates">'
fi

cat <<EOF

<!ENTITY sourceslist-list-format "deb http://deb.debian.org/debian &vendor-codename; main contrib non-free
deb http://security.debian.org &vendor-security-suite; main contrib non-free">
<!ENTITY sourceslist-sources-format "Types: deb
URIs: http://deb.debian.org/debian
Suites: &vendor-codename;
Components: main contrib non-free

Types: deb
URIs: http://security.debian.org
Suites: &vendor-security-suite;
Components: main contrib non-free">
EOF
