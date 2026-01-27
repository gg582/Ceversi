# --- C Build Stage ---
FROM alpine:latest AS c_builder

# Install build dependencies
# Using edge community repository to ensure latest versions of libraries
RUN apk update
RUN apk add --no-cache \
    gcc musl-dev make sqlite-dev openssl-dev cjson-dev uriparser-dev libc-utils linux-headers \
    --repository=https://dl-cdn.alpinelinux.org/alpine/edge/community

WORKDIR /app

# Copy the entire project context
COPY . .

ARG CWIST_REPO="https://github.com/gg582/cwist.git"
ARG CWIST_REF="main"
RUN git clone --depth 1 --branch "${CWIST_REF}" "${CWIST_REPO}" /app/cwist

# 1. Install Headers (Include) manually to system paths
# This resolves compilation issues where Makefiles use relative paths (e.g., -I../include)
# by making headers available globally in /usr/local/include.
RUN cp -r cwist/include/* /usr/local/include/ && \
    cp -r libs/include/* /usr/local/include/ 2>/dev/null || true

# 2. Build Library & Install (Link)
# Build libcwist.a and copy it to /usr/lib so the linker finds it automatically
# without needing specific -L flags.
WORKDIR /app/cwist
RUN make clean && make && \
    cp libcwist.a /usr/lib/

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
COPY --from=c_builder /app/server ./ceversi-backend

COPY public/ ./public/

# Copy root-level templates and fallback assets
COPY index.html.tmpl .
COPY style.css .
COPY script.js .
COPY othello.db .

EXPOSE 31744

ENTRYPOINT ["./ceversi --no-certs"]
