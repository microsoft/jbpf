FROM mcr.microsoft.com/oss/mirror/docker.io/library/ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

RUN echo "*** Installing packages"

# Update package lists and install essential tools
RUN apt update --fix-missing && \
    apt install -y cmake build-essential libboost-dev git \
    libboost-program-options-dev gcovr doxygen libboost-filesystem-dev \
    libasan6 python3 clang-format wget software-properties-common

RUN apt install -y cppcheck

## Emulator
RUN apt install -y python-dev python3-pip
RUN pip3 install ctypesgen

# Install specific versions for CVEs
## CVE-2023-4016
RUN apt install -y procps=2:3.3.16-1ubuntu2.4 --allow-downgrades

## CVE-2024-28085
RUN apt install -y util-linux=2.34-0.1ubuntu9.6 --allow-downgrades

## CVE-2022-1304
RUN apt install -y e2fsprogs=1.45.5-2ubuntu1.2 --allow-downgrades

# Install Clang 12 from official Ubuntu repository
RUN apt install -y clang-12 lldb-12 lld-12

# Set Clang 12 as the default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-12 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-12 100

# Install other necessary packages
RUN apt install -y gcc-multilib libyaml-cpp-dev

# Set the working directory and copy the project files
WORKDIR /jbpf
COPY . /jbpf

# Set the entry point to build the project
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
