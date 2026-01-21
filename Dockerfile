FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libssl-dev \
    libcjson-dev \
    liburiparser-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy local cwist library (Prepared by quick-deploy.sh)
COPY libs/include /usr/local/include
COPY libs/lib /usr/local/lib

# Setup Workdir
WORKDIR /app

# Copy Source
COPY . .

# Compile
RUN make

# Expose Port
EXPOSE 31744

# Run
CMD ["./server"]
