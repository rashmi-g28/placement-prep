FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV AWS_SDK_VERSION=1.11.379

RUN apt-get update && apt-get install -y \
    build-essential cmake git wget unzip libsqlite3-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Build and install AWS SDK for C++
RUN git clone --branch ${AWS_SDK_VERSION} --depth 1 https://github.com/aws/aws-sdk-cpp.git \
    && cd aws-sdk-cpp && mkdir build && cd build \
    && cmake .. -DBUILD_ONLY="bedrock-runtime;sts" -DENABLE_UNITY_BUILD=ON -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc) && make install

COPY . /app

RUN mkdir build && cd build && cmake .. && make -j$(nproc)

EXPOSE 8080

CMD ["./build/server"]
