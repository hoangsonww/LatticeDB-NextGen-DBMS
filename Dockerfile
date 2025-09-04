FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      cmake g++ ninja-build ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j

ENTRYPOINT ["./build/latticedb"]
