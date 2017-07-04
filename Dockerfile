FROM debian:stretch
COPY . /tmp
WORKDIR /tmp
RUN sed -i s#://deb.debian.org#://cdn-fastly.deb.debian.org# /etc/apt/sources.list \
    && apt-get update \
    && adduser --home /home/travis travis --quiet --disabled-login --gecos "" --uid 1000 \
    && env DEBIAN_FRONTEND=noninteractive apt-get install build-essential ccache ninja-build expect curl git -q -y \
    && env DEBIAN_FRONTEND=noninteractive ./prepare-release travis-ci \
    && dpkg-reconfigure ccache \
    && rm -r /tmp/* \
    && apt-get clean
