# - Try to find JSONCPP
# Once done, this will define
#
#  JSONCPP_FOUND - system has JSONCPP
#  JSONCPP_INCLUDE_DIRS - the JSONCPP include directories
#  JSONCPP_LIBRARIES - link these to use JSONCPP

include(FindPackageHandleStandardArgs)

find_path(JSONCPP_INCLUDE_DIR json.h 
PATHS /usr/include/jsoncpp/json
 /usr/local/include/jsoncpp
 /usr/local/include/jsoncpp/json
 /usr/local/jsoncpp/include
 /opt/jsoncpp/include
)

find_library(JSONCPP_LIBRARY libjsoncpp.so 
PATHS /usr/lib 
 /usr/lib/jsoncpp
 /usr/local/lib
 /usr/local/lib/jsoncpp
 /usr/local/jsoncpp/lib
 /opt/jsoncpp/lib
)

find_package_handle_standard_args(JSONCPP  DEFAULT_MSG 
                                  JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY)

mark_as_advanced(JSONCPP_LIBRARY JSONCPP_INCLUDE_DIR)
