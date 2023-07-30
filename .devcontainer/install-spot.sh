#!/usr/bin/env bash

set -e

cd /opt/
wget http://www.lrde.epita.fr/dload/spot/spot-2.11.3.tar.gz
tar xf spot-2.11.3.tar.gz
rm -v spot-2.11.3.tar.gz
cd spot-2.11.3
./configure --disable-python --enable-shared=no --enable-static=yes
make -j8
make install