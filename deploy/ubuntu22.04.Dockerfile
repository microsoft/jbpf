FROM mcr.microsoft.com/mirror/docker/library/ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
ENV CLANG_FORMAT_CHECK=1
ENV CPP_CHECK=1

RUN echo "*** Installing packages"

RUN apt-get clean && apt-get update && \
    apt-get install -y --no-install-recommends \
        cmake build-essential libboost-dev libboost-program-options-dev \
        git gcovr doxygen libboost-filesystem-dev libasan6 python3 pkg-config \
        clang-format cppcheck clang libyaml-cpp-dev python3-dev python3-pip && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
    
RUN if [ "$TARGET_ARCHITECTURE" = "amd64" ]; then \
    apt-get install -y gcc-multilib; \
  elif [ "$TARGET_ARCHITECTURE" = "arm64" ]; then \
    apt-get install -y libc6-dev; \
  fi

## Emulator
RUN pip3 install ctypesgen ruamel.yaml

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
