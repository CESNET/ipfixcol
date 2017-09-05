FROM debian:latest

MAINTAINER Petr Velan <petr.velan@cesnet.cz>

# IPFIXcol dependencies
RUN apt-get update && apt-get install -y \
  autoconf \
  bison \
  build-essential \
  docbook-xsl \
  doxygen \
  flex \
  git \
  liblzo2-dev \
  libtool \
  libsctp-dev \
  libssl1.0-dev \
  libxml2 \
  libxml2-dev \
  pkg-config \
  xsltproc

# checkout IPFIXcol
WORKDIR /root/
RUN git clone --recursive https://github.com/CESNET/ipfixcol.git
WORKDIR /root/ipfixcol/

# select devel branch for installation
RUN git checkout devel;
RUN git submodule update --init --recursive

# install IPFIXcol base
RUN cd base; \
  autoreconf -i; \
  ./configure --with-distro=debian; \
  make clean; \
  make install; \
  ldconfig

# install IPFIXcol plugins
RUN for p in storage/json; do \
    cd plugins/$p; \
    autoreconf -i; \
    ./configure --with-distro=debian; \
	make clean; \
    make install; \
	cd -; \
  done

EXPOSE 4739 4739/udp
VOLUME /etc/ipfixcol/

ENTRYPOINT ["/usr/local/bin/ipfixcol"]
