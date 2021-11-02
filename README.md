# Reproduce https://github.com/aws/aws-sdk-cpp/issues/1779

## Build program
```sh
$ make
g++ -W -Wall -Werror -std=c++11  -c main.cpp -o main.o
g++ -W -Wall -Werror -std=c++11  -c snapshot.cpp -o snapshot.o
g++ -W -Wall -Werror -std=c++11 main.o snapshot.o -o aws-cpp-sdk-1779 -laws-cpp-sdk-ebs -laws-cpp-sdk-core -lpthread

```

## Reproduce issue
```sh
./aws-cpp-sdk-1779 --snapshot "EBS snapshot ID" --outputFile <file name>
```

* Example output:

```sh
$ ./aws-cpp-sdk-1779 --snapshot "snap-025202fa8f7378356" --outputFile temp.out
Process executor is executing....
Executing operation in thread.
Waiting for child PID 23735....
Reduced the priority of the process 23735
ListSnapshotBlocks: snap-025202fa8f7378356 received 60 blocks. Next token , total blocks 60 snapshot size 1GB
ListSnapshotBlocks completed for snap-025202fa8f7378356, total blocks 60 size: 1GB
Received 0 callbacks
Received 40 callbacks
Received 60 callbacks
Finished writing 60 blocks
Process killed by signal: 11.
Child 23735 exited.
```

* Core dump seen from gdb:

```sh
Program received signal SIGSEGV, Segmentation fault.
[Switching to Thread 0x7fffe8ff9700 (LWP 21691)]
0x00007ffff74aef81 in pthread_join () from /lib64/libpthread.so.0
Missing separate debuginfos, use: debuginfo-install cyrus-sasl-lib-2.1.26-23.el7.x86_64 glibc-2.17-324.el7_9.x86_64 keyutils-libs-1.5.8-3.el7.x86_64 krb5-libs-1.15.1-50.el7.x86_64 libcom_err-1.42.9-19.el7.x86_64 libcurl-7.29.0-59.el7_9.1.x86_64 libgcc-4.8.5-44.el7.x86_64 libidn-1.28-4.el7.x86_64 libselinux-2.5-15.el7.x86_64 libssh2-1.8.0-4.el7.x86_64 libstdc++-4.8.5-44.el7.x86_64 nspr-4.25.0-2.el7_9.x86_64 nss-3.53.1-7.el7_9.x86_64 nss-pem-1.0.3-7.el7.x86_64 nss-softokn-3.53.1-6.el7_9.x86_64 nss-softokn-freebl-3.53.1-6.el7_9.x86_64 nss-sysinit-3.53.1-7.el7_9.x86_64 nss-util-3.53.1-1.el7_9.x86_64 openldap-2.4.44-23.el7_9.x86_64 openssl-libs-1.0.2k-21.el7_9.x86_64 pcre-8.32-17.el7.x86_64 sqlite-3.7.17-8.el7_7.1.x86_64 zlib-1.2.7-19.el7_9.x86_64
(gdb) bt
#0  0x00007ffff74aef81 in pthread_join () from /lib64/libpthread.so.0
#1  0x00007ffff78ae97b in aws_thread_join () from /usr/local/lib64/libaws-cpp-sdk-core.so
#2  0x00007ffff7821987 in s_destroy () from /usr/local/lib64/libaws-cpp-sdk-core.so
#3  0x00007ffff781c63f in s_aws_event_loop_group_shutdown_sync () from /usr/local/lib64/libaws-cpp-sdk-core.so
#4  0x00007ffff781c731 in s_event_loop_destroy_async_thread_fn () from /usr/local/lib64/libaws-cpp-sdk-core.so
#5  0x00007ffff78aeb36 in thread_fn () from /usr/local/lib64/libaws-cpp-sdk-core.so
#6  0x00007ffff74adea5 in start_thread () from /lib64/libpthread.so.0
#7  0x00007ffff69b69fd in clone () from /lib64/libc.so.6
(gdb)
```
