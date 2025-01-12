# Builder stage with minimal base image
FROM alpine:3.19 as builder

# Install build dependencies with cleanup in the same layer
RUN apk add --no-cache \
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
    musl-dev

# Create proper i2c-dev.h location
RUN mkdir -p /usr/local/include/linux && \
    cp /usr/include/linux/i2c-dev.h /usr/local/include/linux/

# Install cpp-httplib (header-only library) with minimal git clone depth
RUN git clone --depth 1 https://github.com/yhirose/cpp-httplib.git && \
    mkdir -p /usr/local/include && \
    cp cpp-httplib/httplib.h /usr/local/include/ && \
    rm -rf cpp-httplib

# Build libjson-c from source with optimized flags
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
COPY <<EOF /usr/local/lib/pkgconfig/gloox.pc
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: gloox
Description: C++ library for XMPP clients
Version: 1.0.28
Libs: -L${libdir} -lgloox
Cflags: -I${includedir}
EOF

# Copy and build the application
COPY . /jsonrpc-tcp-srv

RUN cd /jsonrpc-tcp-srv && \
    export BUILDOUT=/output && \
    # Use proper include path for i2c-dev.h
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
    cd / && \
    rm -rf /jsonrpc-tcp-srv

# Create minimal runtime image using alpine
FROM alpine:3.19

# Install only required runtime dependencies
RUN apk add --no-cache \
    libstdc++ \
    libgcc \
    openssl \
    i2c-tools

# Copy built artifacts
COPY --from=builder /output /usr/local

# Copy all necessary libraries
COPY --from=builder /usr/local/lib/libgloox.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libjson-c.so* /usr/local/lib/

# Set environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib

# Copy and set up entrypoint script
COPY start-xmpp-chatbot.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/start-xmpp-chatbot.sh && \
    # Create necessary directories
    mkdir -p /usr/local/lib && \
    # Update library cache
    ldconfig /usr/local/lib || true

ENTRYPOINT ["/usr/local/bin/start-xmpp-chatbot.sh"]
