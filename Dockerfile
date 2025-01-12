# Use a full-featured base image for the build stage
FROM ubuntu:22.04 as builder

# Install build dependencies
RUN apt-get update && \
    apt-get install -y \
    wget \
    tar \
    cmake \
    make \
    g++ \
    git \
    libssl-dev \
    bzip2 \
    autoconf \
    automake \
    libtool \
    pkg-config && \
    rm -rf /var/lib/apt/lists/*

# Install cpp-httplib (header-only library)
RUN git clone https://github.com/yhirose/cpp-httplib.git && \
    mkdir -p /usr/local/include && \
    cp cpp-httplib/httplib.h /usr/local/include/ && \
    rm -rf cpp-httplib

# Build libjson-c from source
RUN wget https://s3.amazonaws.com/json-c_releases/releases/json-c-0.15.tar.gz -O json-c.tar.gz && \
    tar -xzf json-c.tar.gz && \
    cd json-c-0.15 && \
    cmake -H. -Bbuild -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build build -- install -j$(nproc) && \
    cd .. && \
    rm -rf json-c-0.15 json-c.tar.gz

# Build libgloox from source (version 1.0.28) using autotools
RUN wget https://camaya.net/download/gloox-1.0.28.tar.bz2 -O gloox.tar.bz2 && \
    tar -xjf gloox.tar.bz2 && \
    cd gloox-1.0.28 && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install && \
    cd .. && \
    rm -rf gloox-1.0.28 gloox.tar.bz2

# Ensure gloox development files are detectable by pkg-config
RUN echo "prefix=/usr/local" > /usr/local/lib/pkgconfig/gloox.pc && \
    echo "exec_prefix=\${prefix}" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "libdir=\${exec_prefix}/lib" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "includedir=\${prefix}/include" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "Name: gloox" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "Description: C++ library for XMPP clients" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "Version: 1.0.28" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "Libs: -L\${libdir} -lgloox" >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo "Cflags: -I\${includedir}" >> /usr/local/lib/pkgconfig/gloox.pc

# Copy the entire jsonrpc-tcp-srv folder into the Docker image
COPY . /jsonrpc-tcp-srv

# Build jsonrpc-tcp-srv from local files and clean up
RUN cd /jsonrpc-tcp-srv && \
    export BUILDOUT=/output && \
    export LD_LIBRARY_PATH=/output/lib && \
    cmake -H. -BOutput -DCMAKE_INSTALL_PREFIX=$BUILDOUT -DWITH_AI_BOT=ON && \
    cmake --build Output -- install -j$(nproc) && \
    cd / && \
    rm -rf /jsonrpc-tcp-srv  # Clean up the source folder

# Create a minimal runtime image
FROM busybox:latest

# Copy built artifacts from the builder stage
COPY --from=builder /output /usr/local
COPY --from=builder /usr/local/lib /usr/local/lib

# Copy required shared libraries from the builder stage
COPY --from=builder /usr/lib/x86_64-linux-gnu/libstdc++.so.6 /usr/local/lib/
COPY --from=builder /lib/x86_64-linux-gnu/libgcc_s.so.1 /usr/local/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libssl.so.3 /usr/local/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcrypto.so.3 /usr/local/lib/

# Set environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib

# Copy the start script and set the entry point
COPY start-xmpp-chatbot.sh /usr/local/bin/start-xmpp-chatbot.sh
RUN chmod +x /usr/local/bin/start-xmpp-chatbot.sh
ENTRYPOINT ["/usr/local/bin/start-xmpp-chatbot.sh"]
