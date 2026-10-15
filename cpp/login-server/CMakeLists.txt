# Port of ../login-server (com.aionemu.loginserver).
#
# The server runs with ../login-server as working directory, so it uses the same ./config and ./log directories as the Java server.

find_package(OpenSSL REQUIRED)

set(LS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LS_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(LS_JAVA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../login-server")

# client/game server encryption (Blowfish, RSA key pairs): independent of everything else
aion_add_library(aion_loginserver_crypto
	SOURCE_ROOT "${LS_SRC}"
	SOURCE_DIRS aion/loginserver/network/ncrypt
	DEPENDS aion::commons OpenSSL::Crypto
)

# everything else except main()
aion_add_library(aion_loginserver_lib
	SOURCE_ROOT "${LS_SRC}"
	SOURCE_DIRS aion/loginserver
	EXCLUDE_REGEX "/aion/loginserver/network/ncrypt/"
	DEPENDS aion_loginserver_crypto aion::commons OpenSSL::Crypto
)
target_compile_definitions(aion_loginserver_lib PUBLIC AION_LOGINSERVER_JAVA_DIR="${LS_JAVA_DIR}")

add_executable(aion_login_server "${LS_SRC}/main.cpp")
target_link_libraries(aion_login_server PRIVATE aion_loginserver_lib aion::compiler_options)
set_target_properties(aion_login_server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${LS_JAVA_DIR}")

aion_add_tests(aion_loginserver_crypto_tests "${LS_TESTS}/crypto" DEPENDS aion_loginserver_crypto)
aion_add_tests(aion_loginserver_data_tests "${LS_TESTS}/data" DEPENDS aion_loginserver_lib)
aion_add_tests(aion_loginserver_tests "${LS_TESTS}/server" DEPENDS aion_loginserver_lib)

# header-only test helpers shared by the test targets (e.g. the client side of the Aion login protocol), included as "support/..."
foreach(test_target aion_loginserver_crypto_tests aion_loginserver_data_tests aion_loginserver_tests)
	if(TARGET ${test_target})
		target_include_directories(${test_target} PRIVATE "${LS_TESTS}")
		target_compile_definitions(${test_target} PRIVATE AION_LOGINSERVER_JAVA_DIR="${LS_JAVA_DIR}")
	endif()
endforeach()
