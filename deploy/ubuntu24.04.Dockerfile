FROM mcr.microsoft.com/mirror/docker/library/ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
ENV CPP_CHECK=1

RUN echo "*** Installing packages"
RUN apt update --fix-missing
RUN apt install -y cmake build-essential libboost-dev libboost-program-options-dev \
    gcovr doxygen libboost-filesystem-dev libasan6 python3

RUN apt install -y clang-format cppcheck
RUN apt install -y clang gcc-multilib
RUN apt install -y libyaml-cpp-dev

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
