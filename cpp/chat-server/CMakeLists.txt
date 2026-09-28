# Port of ../chat-server (com.aionemu.chatserver).
#
# The server runs with ../chat-server as working directory, so it uses the same ./config and ./log directories as the Java server.
# Built only with -DAION_BUILD_CHAT_SERVER=ON (top-level CMakeLists.txt).

find_package(OpenSSL REQUIRED)

set(CS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(CS_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(CS_JAVA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../chat-server")

# everything except main() (OpenSSL: SHA-256 of the chat token, Java: MessageDigest)
aion_add_library(aion_chatserver_lib
	SOURCE_ROOT "${CS_SRC}"
	SOURCE_DIRS aion/chatserver
	DEPENDS aion::commons OpenSSL::Crypto
)

add_executable(aion_chat_server "${CS_SRC}/main.cpp")
target_link_libraries(aion_chat_server PRIVATE aion_chatserver_lib aion::compiler_options)
set_target_properties(aion_chat_server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CS_JAVA_DIR}")

# unit tests of the model, the configs and the helpers
aion_add_tests(aion_chatserver_model_tests "${CS_TESTS}/model" DEPENDS aion_chatserver_lib)
# packet byte vectors, framing, both network sides and the services, against an in-process server on run-time ports
aion_add_tests(aion_chatserver_network_tests "${CS_TESTS}/network" DEPENDS aion_chatserver_lib)
# ChatLogDAO against the test schema (AION_TEST_CS_DATABASE_URL)
aion_add_tests(aion_chatserver_data_tests "${CS_TESTS}/data" DEPENDS aion_chatserver_lib)
# the real aion_chat_server executable with a fake game server and two fake clients (needs the test schema as well)
aion_add_tests(aion_chatserver_e2e_tests "${CS_TESTS}/e2e" DEPENDS aion_chatserver_lib)

# header-only test helpers shared by the test targets (fake clients, packet builders, the test database), included as "support/..."
foreach(test_target aion_chatserver_model_tests aion_chatserver_network_tests aion_chatserver_data_tests aion_chatserver_e2e_tests)
	if(TARGET ${test_target})
		target_include_directories(${test_target} PRIVATE "${CS_TESTS}")
		target_compile_definitions(${test_target} PRIVATE AION_CHATSERVER_JAVA_DIR="${CS_JAVA_DIR}")
	endif()
endforeach()
if(TARGET aion_chatserver_e2e_tests)
	target_compile_definitions(aion_chatserver_e2e_tests PRIVATE AION_CHAT_SERVER_EXECUTABLE="$<TARGET_FILE:aion_chat_server>")
	add_dependencies(aion_chatserver_e2e_tests aion_chat_server)
endif()

# CTest labels: every chat server test carries "chatserver"; the ones that need the test database (and read chat-server/sql from the Java tree)
# also carry "realdata", so `ctest -L chatserver -LE realdata` is the run without a database.
aion_set_discovered_test_properties(aion_chatserver_model_tests REGEX ".*" PROPERTIES LABELS chatserver)
aion_set_discovered_test_properties(aion_chatserver_network_tests REGEX ".*" PROPERTIES LABELS chatserver)
aion_set_discovered_test_properties(aion_chatserver_data_tests REGEX ".*" PROPERTIES LABELS "chatserver;realdata")
aion_set_discovered_test_properties(aion_chatserver_e2e_tests REGEX ".*" PROPERTIES LABELS "chatserver;realdata" TIMEOUT 180)
