# Port of ../commons (com.aionemu.commons). Split into several libraries so that subsystems can be built and tested independently.

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(unofficial-libmariadb CONFIG REQUIRED)

set(COMMONS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(COMMONS_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")

# utils + logging: used by everything
aion_add_library(aion_commons_core
	SOURCE_ROOT "${COMMONS_SRC}"
	SOURCE_DIRS aion/commons/utils aion/commons/logging
	DEPENDS fmt::fmt spdlog::spdlog magic_enum::magic_enum cpr::cpr nlohmann_json::nlohmann_json asio::asio
)
target_include_directories(aion_commons_core PUBLIC "${AION_GENERATED_INCLUDE_DIR}")

# .properties loading and @Property-style binding, plus the config classes of commons itself
aion_add_library(aion_commons_configuration
	SOURCE_ROOT "${COMMONS_SRC}"
	SOURCE_DIRS aion/commons/configuration aion/commons/configs
	DEPENDS aion_commons_core
)

# JDBC-like database access over MariaDB Connector/C
aion_add_library(aion_commons_database
	SOURCE_ROOT "${COMMONS_SRC}"
	SOURCE_DIRS aion/commons/database
	DEPENDS aion_commons_core unofficial::libmariadb
)

# connection handling, packet framing and processing over Asio
aion_add_library(aion_commons_network
	SOURCE_ROOT "${COMMONS_SRC}"
	SOURCE_DIRS aion/commons/network
	DEPENDS aion_commons_core asio::asio
)

add_library(aion_commons INTERFACE)
target_link_libraries(aion_commons INTERFACE aion_commons_core aion_commons_configuration aion_commons_database aion_commons_network)
add_library(aion::commons ALIAS aion_commons)

aion_add_tests(aion_commons_core_tests "${COMMONS_TESTS}/core" DEPENDS aion_commons_core)
aion_add_tests(aion_commons_configuration_tests "${COMMONS_TESTS}/configuration" DEPENDS aion_commons_configuration)
aion_add_tests(aion_commons_database_tests "${COMMONS_TESTS}/database" DEPENDS aion_commons_database)
aion_add_tests(aion_commons_network_tests "${COMMONS_TESTS}/network" DEPENDS aion_commons_network)
