# Best security principles


## Secure build process

*jbpf* build process depends on multiple open source projects that are downloaded from the Internet. 
Some are consumed in form of packages (`<distro_name>.Dockerfile`, see also [here](./integrate_lib.md)), and some as git [submodules](../.gitmodules).
In our pipelines, we always test with the latest versions of the packages.
The submodule versions are fixed, but we try to update them frequently, to test with the newest releases and patches. 

However, when integrating in your own project, you will want to set up your own security process for the supply chain, and in particular:
- Fix the package versions when installing it. For example, in Ubuntu, use: `apt-get install <package>=<version>`
- Fix the versions of the submodules. 
You will want to regularly monitor dependencies for security risks and update accordingly. 


## Secure codelet storage

Since the codelets get detailed access to and potentially control of the host application, it is very important to manage them safely. 
One of the [design decisions](./life_cycle_management.md) is that only codelet stored on a local secure store can be loaded. 
It is a responsibility of the application developers to provide a local, secure codelet storage.

The best way to create a secure storage depends on the environment. 
This document is only meant to provide suggestions on how to achieve this, not a solution. 
On a Linux deployment, one may want to create a directory and limit access to it. 
In a Kubernetes environment, one may want to create a secure persistent volume, map it to a pod as a directory, and store codelets there. 
Many cloud environments offer secure and encrypted storage classes for Kubernetes that can be used for this purpose. 




