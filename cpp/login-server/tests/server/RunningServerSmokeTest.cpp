// Smoke test against a separately started login server (aion_login_server.exe). It runs only if AION_TEST_RUNNING_LS_PORT is set to the client
// port of that server (e.g. 2106); AION_TEST_RUNNING_LS_ACCOUNT names the account to log in with (auto-created by the server, default
// "smoketest").

#include <cstdlib>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

#include "ServerTestUtils.h"

namespace aion::loginserver::test {
namespace {

TEST(RunningServerSmokeTest, FakeClientLogsIn) {
	const char* port = std::getenv("AION_TEST_RUNNING_LS_PORT");
	if (!port || !*port)
		GTEST_SKIP() << "AION_TEST_RUNNING_LS_PORT is not set";
	const char* accountEnv = std::getenv("AION_TEST_RUNNING_LS_ACCOUNT");
	const std::string account = accountEnv && *accountEnv ? accountEnv : "smoketest";

	AionTestClient client(static_cast<uint16_t>(std::stoi(port)));
	auto init = client.readInit();
	EXPECT_EQ(init.protocolRevision, 0xc621);
	std::cout << "SM_INIT: session id " << init.sessionId << std::endl;
	client.sendPacket(AionTestClient::buildCM_AUTH_GG(init.sessionId));
	client.expectPacket(0x0b);
	std::cout << "SM_AUTH_GG received" << std::endl;

	PacketReader ok(client.login(account, "smokepassword", 0x03));
	ok.C();
	int32_t accountId = ok.D();
	int32_t loginOk = ok.D();
	std::cout << "SM_LOGIN_OK: account id " << accountId << std::endl;

	client.sendPacket(AionTestClient::buildCM_SERVER_LIST(accountId, loginOk));
	PacketReader list(client.expectPacket(0x04));
	list.C();
	std::cout << "SM_SERVER_LIST: " << int(list.C()) << " game servers" << std::endl;
}

} // namespace
} // namespace aion::loginserver::test
