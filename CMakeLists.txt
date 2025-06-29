cmake_minimum_required(VERSION 3.10)
project(WarehouseManagementSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ✅ MySQL Connector/C++ include path (update if needed)
include_directories(/usr/local/mysql-connector-c++-9.3.0/include/jdbc)

# ✅ MySQL Connector/C++ library path (update if needed)
link_directories(/usr/local/mysql-connector-c++-9.3.0/lib64)

# ✅ Your only source file
add_executable(warehouse main.cpp)

# ✅ Link MySQL Connector/C++ library
target_link_libraries(warehouse mysqlcppconn)
