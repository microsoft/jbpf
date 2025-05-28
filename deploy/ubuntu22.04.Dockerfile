FROM mcr.microsoft.com/mirror/docker/library/ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
ENV CLANG_FORMAT_CHECK=1
ENV CPP_CHECK=1

RUN echo "*** Installing packages"

RUN apt-get clean && apt-get update --fix-missing && \
    apt-get install -y --no-install-recommends \
        cmake build-essential libboost-dev libboost-program-options-dev \
        wget gcovr doxygen libboost-filesystem-dev libasan6 python3 \
        clang-format cppcheck clang gcc-multilib libyaml-cpp-dev git && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
