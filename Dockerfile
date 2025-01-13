# Builder stage with minimal base image
FROM alpine:3.19 as builder

# Install build dependencies with cleanup in the same layer
RUN apk add --no-cache --virtual .build-deps \
    wget \
    tar \
    cmake \
    make \
    g++ \
    git \
    openssl-dev \
    bzip2 \
    autoconf \
    automake \
    libtool \
    pkgconfig \
    linux-headers \
    i2c-tools-dev \
    musl-dev && \
    mkdir -p /usr/local/include/linux && \
    cp /usr/include/linux/i2c-dev.h /usr/local/include/linux/

# Install cpp-httplib (header-only library)
RUN git clone --depth 1 https://github.com/yhirose/cpp-httplib.git && \
    mkdir -p /usr/local/include && \
    cp cpp-httplib/httplib.h /usr/local/include/ && \
    rm -rf cpp-httplib

# Build libjson-c from source
RUN wget -q https://s3.amazonaws.com/json-c_releases/releases/json-c-0.15.tar.gz && \
    tar -xzf json-c-0.15.tar.gz && \
    cd json-c-0.15 && \
    cmake -H. -Bbuild \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_STATIC_LIBS=OFF && \
    cmake --build build -- install -j$(nproc) && \
    cd .. && \
    rm -rf json-c-0.15 json-c-0.15.tar.gz

# Build libgloox with size optimizations
RUN wget -q https://camaya.net/download/gloox-1.0.28.tar.bz2 && \
    tar -xjf gloox-1.0.28.tar.bz2 && \
    cd gloox-1.0.28 && \
    CXXFLAGS="-Os -ffunction-sections -fdata-sections" \
    LDFLAGS="-Wl,--gc-sections" \
    ./configure --prefix=/usr/local \
        --disable-static \
        --enable-shared \
        --without-examples \
        --without-tests && \
    make -j$(nproc) && \
    make install && \
    cd .. && \
    rm -rf gloox-1.0.28 gloox-1.0.28.tar.bz2

# Configure pkg-config for gloox
RUN echo 'prefix=/usr/local' > /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'exec_prefix=${prefix}' >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'libdir=${exec_prefix}/lib' >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'includedir=${prefix}/include' >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'Name: gloox' >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'Description: C++ library for XMPP clients' >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'Version: 1.0.28' >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'Libs: -L${libdir} -lgloox' >> /usr/local/lib/pkgconfig/gloox.pc && \
    echo 'Cflags: -I${includedir}' >> /usr/local/lib/pkgconfig/gloox.pc

# Copy and build the application
COPY . /jsonrpc-tcp-srv

# Create wrapper for tcp-json-rpc-client.cpp to include missing headers
RUN echo '#include <sys/select.h>' > /tmp/select_wrapper.h && \
    echo '#include <sys/time.h>' >> /tmp/select_wrapper.h && \
    cat /jsonrpc-tcp-srv/utils/tcp-json-rpc-client/tcp-json-rpc-client.cpp >> /tmp/select_wrapper.h && \
    mv /tmp/select_wrapper.h /jsonrpc-tcp-srv/utils/tcp-json-rpc-client/tcp-json-rpc-client.cpp

RUN cd /jsonrpc-tcp-srv && \
    export BUILDOUT=/output && \
    export CPATH=/usr/local/include && \
    cmake -H. -BOutput \
        -DCMAKE_INSTALL_PREFIX=$BUILDOUT \
        -DWITH_AI_BOT=ON \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DCMAKE_CXX_FLAGS="-Os -ffunction-sections -fdata-sections -Wno-error=cpp -I/usr/local/include" \
        -DCMAKE_C_FLAGS="-Os -ffunction-sections -fdata-sections -Wno-error=cpp -I/usr/local/include" \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections" && \
    cmake --build Output -- install -j$(nproc) && \
    strip $BUILDOUT/bin/* && \
    strip /usr/local/lib/libgloox.so* && \
    strip /usr/local/lib/libjson-c.so* && \
    cd / && \
    rm -rf /jsonrpc-tcp-srv && \
    apk del .build-deps

# Create minimal runtime image
FROM alpine:3.19

# Install only required runtime dependencies
RUN apk add --no-cache \
    libstdc++ \
    libgcc \
    openssl \
    dropbear \
    openssh-client && \
    rm -rf /var/cache/apk/*

# Copy built artifacts
COPY --from=builder /output /usr/local
COPY --from=builder /usr/local/lib/libgloox.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libjson-c.so* /usr/local/lib/

# Remove unnecessary CMake files
RUN rm -rf /usr/local/lib/cmake

# Set environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib

# Copy and set up entrypoint script
COPY start-xmpp-chatbot.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/start-xmpp-chatbot.sh && \
    mkdir -p /usr/local/lib && \
    ldconfig /usr/local/lib || true

# Expose SSH port (optional)
#EXPOSE 22

ENTRYPOINT ["/usr/local/bin/start-xmpp-chatbot.sh"]
