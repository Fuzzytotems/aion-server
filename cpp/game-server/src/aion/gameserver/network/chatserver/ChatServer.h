#pragma once

#include <cstdint>
#include <memory>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/chatserver/fwd.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::commons::network {
class NioServer;
}

namespace aion::gameserver::network::chatserver {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) like LoginServer. The public address is the byte array Java stores and returns
 * (`Ptr<Array<int8_t>>`, hub-headers.md §6); the NioServer is the commons server object owned by GameServer (plain pointer).
 *
 * @author ATracer, Neon
 */
class ChatServer : public runtime::Immortal {
private:
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> publicIp;
	runtime::Field<int32_t> publicPort{0};
	runtime::Field<std::shared_ptr<ChatServerConnection>> csCon{};
	// fieldmap.toml: NioServer is the commons server object (not RefCounted), owned by GameServer for the whole run
	runtime::Field<commons::network::NioServer*> nioServer{};

	/** Prevent instantiation. */
	ChatServer();
	~ChatServer();

public:
	static ChatServer& getInstance();

	void connect(commons::network::NioServer& nioServer);

	void disconnect();

	void reconnect();

	bool isUp();

	void setPublicAddress(runtime::Ptr<runtime::Array<int8_t>> ip, int32_t port);

	runtime::Ptr<runtime::Array<int8_t>> getPublicIP();

	int32_t getPublicPort();

	void sendPlayerLoginRequest(model::gameobjects::player::Player& player);

private:
	/** C++ only: the link if it is up (Java isUp()), read once, so callers send through the connection they checked (null when down) */
	std::shared_ptr<ChatServerConnection> upConnection();

public:

	void sendPlayerLogout(model::gameobjects::player::Player& player);

	void sendPlayerGagPacket(int32_t playerObjId, int64_t gagTime);
};

} // namespace aion::gameserver::network::chatserver
