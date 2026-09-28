#include "aion/gameserver/network/aion/GameConnectionFactoryImpl.h"

#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/sequrity/FloodManager.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.GameConnectionFactoryImpl");

namespace {

using configs::network::NetworkConfig;

runtime::Ref<sequrity::FloodManager> createFloodAcceptor() {
	if (!NetworkConfig::ENABLE_FLOOD_CONNECTIONS.load())
		return nullptr;
	runtime::Ref<sequrity::FloodManager::FloodFilter> shortPeriod =
		sequrity::FloodManager::FloodFilter::create(NetworkConfig::Flood_SWARN.load(), NetworkConfig::Flood_SReject.load(), NetworkConfig::Flood_STick.load());
	runtime::Ref<sequrity::FloodManager::FloodFilter> longPeriod =
		sequrity::FloodManager::FloodFilter::create(NetworkConfig::Flood_LWARN.load(), NetworkConfig::Flood_LReject.load(), NetworkConfig::Flood_LTick.load());
	return sequrity::FloodManager::create(NetworkConfig::Flood_Tick.load(), {shortPeriod, longPeriod});
}

/** Java: socket.socket().getInetAddress().getHostAddress() (IPv4-mapped addresses as IPv4, like AConnection::getIP) */
std::string hostAddressOf(const asio::ip::tcp::socket& socket) {
	asio::error_code error;
	asio::ip::tcp::endpoint endpoint = socket.remote_endpoint(error);
	if (error)
		return {};
	asio::ip::address address = endpoint.address();
	if (address.is_v6() && address.to_v6().is_v4_mapped())
		return asio::ip::make_address_v4(asio::ip::v4_mapped, address.to_v6()).to_string();
	return address.to_string();
}

} // namespace

GameConnectionFactoryImpl::GameConnectionFactoryImpl() : floodAcceptor(createFloodAcceptor()) {
}

GameConnectionFactoryImpl::~GameConnectionFactoryImpl() = default;

runtime::Ref<GameConnectionFactoryImpl> GameConnectionFactoryImpl::create() {
	return runtime::makeRef<GameConnectionFactoryImpl>();
}

std::shared_ptr<AionConnection> GameConnectionFactoryImpl::create(asio::ip::tcp::socket socket, commons::network::NioServer& server) {
	if (NetworkConfig::ENABLE_FLOOD_CONNECTIONS.load() && floodAcceptor) { // C++: also checks the acceptor (Java NPE if enabled by a reload)
		runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
		const std::string host = hostAddressOf(socket);
		const sequrity::FloodManager::Result isFlooding = floodAcceptor->isFlooding(host, true);
		switch (isFlooding) {
			case sequrity::FloodManager::Result::REJECTED:
				log.warn("Rejected connection from " + host);
				return nullptr;
			case sequrity::FloodManager::Result::WARNED:
				log.warn("Connection over warn limit from " + host);
				break;
			default:
				break;
		}
	}

	return std::make_shared<AionConnection>(std::move(socket), server);
}

commons::network::ConnectionFactory GameConnectionFactoryImpl::toConnectionFactory() {
	runtime::Ref<GameConnectionFactoryImpl> self(*this);
	return [self](asio::ip::tcp::socket socket, commons::network::NioServer& server) -> std::shared_ptr<commons::network::AConnectionBase> {
		return self->create(std::move(socket), server);
	};
}

} // namespace aion::gameserver::network::aion
