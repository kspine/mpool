# mpool
MySQL Connection Pool Proxy
## Required Dependencies
[jsoncpp](https://github.com/open-source-parsers/jsoncpp)  
## Installation
**Recommended Install Directory: /opt/mpool**   
$mkdir build  
$cd build  
### Debug build
$cmake -DCMAKE_INSTALL_PREFIX=/opt/mpool -DCMAKE_BUILD_TYPE=Debug ..
### Release build
$cmake -DCMAKE_INSTALL_PREFIX=/opt/mpool -DCMAKE_BUILD_TYPE=Release ..
$make  
#make install  

## Running
$cd /opt/mpool/bin  
$./mpool -h  

# Clients
[PHP Client Sample](https://github.com/leyley/mpool/wiki/PHP-Client-Sample)

# Build on CentOS 5
[How to build MPool on CentOS 5](https://github.com/leyley/mpool/wiki/Build-MPool-on-CentOS-5)
