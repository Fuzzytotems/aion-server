#include "aion/chatserver/configs/network/NetworkConfig.h"

#include "aion/commons/configuration/ConfigurableProcessor.h"

namespace aion::chatserver::configs::network {

void NetworkConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	p.bind("chatserver.network.client.connect_address", CLIENT_CONNECT_ADDRESS, "0.0.0.0:10241");
	p.bind("chatserver.network.client.socket_address", CLIENT_SOCKET_ADDRESS, "0.0.0.0:10241");
	p.bind("chatserver.network.gameserver.socket_address", GAMESERVER_SOCKET_ADDRESS, "0.0.0.0:9021");
	p.bind("chatserver.network.gameserver.password", GAMESERVER_PASSWORD, "");
	p.bind("chatserver.network.nio.threads", NIO_READ_WRITE_THREADS, "1");
}

} // namespace aion::chatserver::configs::network
