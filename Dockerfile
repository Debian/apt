FROM ubuntu:trusty
COPY . /tmp
WORKDIR /tmp
RUN sed -i s#://deb.debian.org#://cdn-fastly.deb.debian.org# /etc/apt/sources.list \
    && apt-get update \
    && adduser --home /home/travis travis --quiet --disabled-login --gecos "" --uid 1000 \
    && env DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends build-essential ccache expect-dev fakeroot libfile-fcntllock-perl wget curl git dpkg-dev debhelper libdb-dev gettext libcurl4-gnutls-dev zlib1g-dev libbz2-dev xsltproc docbook-xsl docbook-xml po4a autotools-dev autoconf automake doxygen debiandoc-sgml stunnel4 -q -y \
    && dpkg-reconfigure ccache \
    && rm -r /tmp/* \
    && apt-get clean
