FROM quay.io/pypa/manylinux2014_x86_64

# Image derived from standard manylinux Python wheel building env, with conan added.
# Wraps conan in a bash script setting the appropriate PYTHONPATH.
# If building the wheel for Python2, conan will fail to run as it will use the wrong
# libs.

RUN yum install -y python3 python27-python-pip glibc-headers python2-devel python3-devel
RUN /usr/bin/python3 -m ensurepip --upgrade
RUN PYTHONPATH=$PYTHONPATH:/opt/rh/python27/root/usr/lib/python2.7/site-packages /usr/bin/python2.7 -m pip install --upgrade 'setuptools<45.0.0' wheel
RUN  /usr/bin/python3 -m pip install --upgrade conan wheel
RUN mv /usr/local/bin/conan /usr/local/bin/conan_
COPY conan_workaround /usr/local/bin/conan
RUN chmod +x /usr/local/bin/conan
