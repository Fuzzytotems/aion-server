#pragma once

#include <memory>

#include "aion/commons/network/ConnectionFactory.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/sequrity/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

/**
 * ConnectionFactory implementation that will be creating AionConnections
 * <p>
 * C++: RefCounted (fieldmap K4). commons::network::ConnectionFactory is a std::function: toConnectionFactory() adapts create() for ServerCfg
 * (the adapter retains this factory).
 *
 * @author -Nemesiss-
 */
class GameConnectionFactoryImpl : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	/** null if NetworkConfig.ENABLE_FLOOD_CONNECTIONS was off when the factory was created */
	const runtime::Ref<sequrity::FloodManager> floodAcceptor;

protected:
	GameConnectionFactoryImpl();
	~GameConnectionFactoryImpl() override;

public:
	/** Java: new GameConnectionFactoryImpl() */
	static runtime::Ref<GameConnectionFactoryImpl> create();

	/**
	 * Create a new AionConnection instance.
	 *
	 * @param socket
	 *          that new AionConnection instance will represent.
	 * @param server
	 *          to which new connection will be registered.
	 * @return a new instance of AionConnection, or null to reject the connection (flooding host)
	 */
	std::shared_ptr<AionConnection> create(asio::ip::tcp::socket socket, commons::network::NioServer& server);

	/** C++ only: this factory as the commons ConnectionFactory of a ServerCfg */
	commons::network::ConnectionFactory toConnectionFactory();
};

} // namespace aion::gameserver::network::aion
