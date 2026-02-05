# --- C Build Stage ---
FROM alpine:latest AS c_builder

# Install build dependencies
# Using edge community repository to ensure latest versions of libraries
RUN apk update
RUN apk add --no-cache \
    tcc gcc musl-dev make sqlite-dev openssl-dev cjson-dev uriparser-dev git libc-utils linux-headers \
    --repository=https://dl-cdn.alpinelinux.org/alpine/edge/community

WORKDIR /app

# Copy the entire project context
COPY . .

ARG LIBTTAK_REPO="https://github.com/gg582/libttak.git"
ARG LIBTTAK_REF="main"

RUN git clone --depth 1 --branch "${LIBTTAK_REF}" "${LIBTTAK_REPO}" /app/libttak
WORKDIR /app/libttak
RUN make clean && make && \
    make install

ARG CWIST_REPO="https://github.com/gg582/cwist.git"
ARG CWIST_REF="main"
RUN git clone --depth 1 --branch "${CWIST_REF}" "${CWIST_REPO}" /app/cwist

# 1. Install Headers (Include) manually to system paths
# This resolves compilation issues where Makefiles use relative paths (e.g., -I../include)
# by making headers available globally in /usr/local/include.

# 2. Build Library & Install (Link)
# Build libcwist.a and copy it to /usr/lib so the linker finds it automatically
# without needing specific -L flags.
WORKDIR /app/cwist
RUN make clean && make && \
    make install

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

EXPOSE 31744

ENTRYPOINT ["./ceversi", "--no-certs"]
