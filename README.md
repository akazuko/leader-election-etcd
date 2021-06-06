# Leader election using etcd

This is a sample application to show how leader application can be done for a distributed application using etcd. Here we use etcd v3 API. The code in this repo is in c++, however the same can be easily ported to the langige of your choice.

To compile, run the following command:

```
g++ -std=c++14 -I /usr/local/include /usr/local/lib/libetcd-cpp-api.dylib main.cpp

// run multiple instances of the executable and check by killing various instances to see how the election happens and when the quorum breaks
```