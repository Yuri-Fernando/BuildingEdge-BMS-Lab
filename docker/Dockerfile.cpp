FROM gcc:14-bookworm AS builder
RUN apt-get update && apt-get install -y --no-install-recommends cmake && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY cpp/ ./cpp/
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" && \
    cmake --build build --parallel 2 && ctest --test-dir build --output-on-failure
FROM debian:bookworm-slim
RUN useradd --system --no-create-home bms
COPY --from=builder /src/build/bms-edge /usr/local/bin/bms-edge
USER bms
ENTRYPOINT ["bms-edge"]
