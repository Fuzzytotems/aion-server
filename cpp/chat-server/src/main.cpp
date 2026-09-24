// Entry point of the chat server (Java: com.aionemu.chatserver.ChatServer.main).
// Run it with ../chat-server (the Java module directory) as working directory, so ./config and ./log resolve like for the Java server.
// Configuration properties can be overridden with -D<key>=<value> arguments, and --stop-file=<path> shuts the server down when the file
// appears (see ChatServer::main).

#include <string_view>
#include <vector>

#include "aion/chatserver/ChatServer.h"

int main(int argc, char* argv[]) {
	std::vector<std::string_view> args;
	for (int i = 1; i < argc; i++)
		args.emplace_back(argv[i]);
	return aion::chatserver::ChatServer::main(args);
}
