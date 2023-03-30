ARG ARCH=x86_64
FROM quay.io/pypa/manylinux2014_${ARCH}:latest

# Image derived from standard manylinux Python wheel building env, with conan added.
# Wraps conan in a bash script setting the appropriate PYTHONPATH.
# If building the wheel for Python2, conan will fail to run as it will use the wrong
# libs.

RUN yum install -y python3 glibc-headers python3-devel
RUN /usr/bin/python3 -m ensurepip --upgrade
RUN  /usr/bin/python3 -m pip install --upgrade conan==1.* wheel
RUN conan profile new default --detect
RUN conan remote add -f conancenter https://center.conan.io
