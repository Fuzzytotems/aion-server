// Entry point of the login server (Java: com.aionemu.loginserver.LoginServer.main).
// Run it with ../login-server (the Java module directory) as working directory, so ./config and ./log resolve like for the Java server.
// Configuration properties can be overridden with -D<key>=<value> arguments (see LoginServer::main).

#include <string_view>
#include <vector>

#include "aion/loginserver/LoginServer.h"

int main(int argc, char* argv[]) {
	std::vector<std::string_view> args;
	for (int i = 1; i < argc; i++)
		args.emplace_back(argv[i]);
	return aion::loginserver::LoginServer::main(args);
}
