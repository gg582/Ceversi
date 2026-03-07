# --- C Build Stage ---
FROM alpine:latest AS c_builder

# Install build dependencies
# Using edge community repository to ensure latest versions of libraries
RUN apk update
RUN apk add --no-cache \
    tcc clang lld musl-dev make sqlite-dev openssl-dev cjson-dev uriparser-dev git libc-utils linux-headers \
    --repository=https://dl-cdn.alpinelinux.org/alpine/edge/community

WORKDIR /app

# Copy the entire project context
COPY . .

ARG LIBTTAK_REPO="https://github.com/gg582/libttak.git"
ARG LIBTTAK_REF="main"

RUN git clone --depth 1 --branch "${LIBTTAK_REF}" "${LIBTTAK_REPO}" /app/libttak
WORKDIR /app/libttak
RUN make clean && CC=tcc LDFLAGS="${LDFLAGS} -Wl,--no-eh-frame-hdr -fuse-ld=lld" make && \
    CC=tcc make install

ARG CWIST_REPO="https://github.com/gg582/cwist.git"
ARG CWIST_REF="main"
RUN git clone --depth 1 --branch "${CWIST_REF}" "${CWIST_REPO}" /app/cwist
RUN git -C /app/cwist submodule update --init --recursive

# 1. Install Headers (Include) manually to system paths
# This resolves compilation issues where Makefiles use relative paths (e.g., -I../include)
# by making headers available globally in /usr/local/include.

# 2. Build Library & Install (Link)
# Build libcwist.a and copy it to /usr/lib so the linker finds it automatically
# without needing specific -L flags.
WORKDIR /app/cwist
RUN printf '#!/bin/sh\n/usr/bin/tcc -std=gnu17 "$@"\n' > /usr/local/bin/tcc && \
    chmod +x /usr/local/bin/tcc && \
    rm -rf lib/libttak && mkdir -p lib && \
    cp -r /app/libttak lib/libttak && \
    LDFLAGS="${LDFLAGS} -fuse-ld=lld -Wl,--no-eh-frame-hdr" CC=tcc make && CC=tcc make install && \
    rm /usr/local/bin/tcc

# 3. Build Main Server
# Navigate back to root to build the backend server binary
WORKDIR /app
RUN make clean && make


# --- Final Run Stage ---
FROM alpine:latest

# Install runtime dependencies required for the C server
RUN apk add --no-cache \
    sqlite-libs openssl cjson uriparser libgcc \
    --repository=https://dl-cdn.alpinelinux.org/alpine/edge/community

WORKDIR /app

# Copy the compiled C backend binary
COPY --from=c_builder /app/server ./ceversi

COPY public/ ./public/

# Copy root-level templates and fallback assets
COPY index.html.tmpl .
COPY style.css .
COPY script.js .

# Ensure the runtime image always has a database file to initialize even when
# developers haven't pre-created ./othello.db locally.
RUN touch othello.db

EXPOSE 31744

ENTRYPOINT ["./ceversi", "--no-certs"]
