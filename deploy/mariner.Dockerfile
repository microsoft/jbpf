FROM mcr.microsoft.com/azurelinux/base/core:3.0

RUN echo "*** Installing packages"
RUN tdnf upgrade tdnf --refresh -y
RUN tdnf -y update
RUN tdnf -y install build-essential cmake git
RUN tdnf -y install yaml-cpp-devel yaml-cpp-static boost-devel gcovr clang python3
RUN tdnf -y install doxygen
## clang-format
RUN tdnf -y install clang-tools-extra
## emulator
RUN tdnf -y install python3-devel python3-pip
RUN pip3 install ctypesgen pyyaml regex 

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
