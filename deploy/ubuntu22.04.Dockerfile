FROM mcr.microsoft.com/mirror/docker/library/ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
ENV CLANG_FORMAT_CHECK=1
ENV CPP_CHECK=1

RUN echo "*** Installing packages"
RUN apt update --fix-missing
RUN apt install -y cmake build-essential libboost-dev git libboost-program-options-dev \
    wget gcovr doxygen libboost-filesystem-dev libasan6 python3

RUN apt install -y clang-format cppcheck
RUN apt install -y clang 
RUN if [ "$TARGET_ARCHITECTURE" = "amd64" ]; then \
    apt-get install -y gcc-multilib; \
  elif [ "$TARGET_ARCHITECTURE" = "arm64" ]; then \
    apt-get install -y libc6-dev; \
  fi
RUN apt install -y libyaml-cpp-dev

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
