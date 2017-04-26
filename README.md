# mpool
MySQL Connection Pool Proxy
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