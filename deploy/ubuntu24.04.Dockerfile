FROM mcr.microsoft.com/mirror/docker/library/ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
ENV CPP_CHECK=1

RUN echo "*** Installing packages"
RUN apt update --fix-missing
RUN apt install -y cmake build-essential libboost-dev git libboost-program-options-dev \
    gcovr doxygen libboost-filesystem-dev libasan6 python3 libyaml-cpp-dev

RUN apt install -y clang-format cppcheck
RUN apt install -y clang
RUN if [ "$TARGET_ARCHITECTURE" = "amd64" ]; then \
    apt-get install -y gcc-multilib; \
  elif [ "$TARGET_ARCHITECTURE" = "arm64" ]; then \
    apt-get install -y libc6-dev; \
  fi

RUN apt-get update && apt-get install -y pkg-config

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
