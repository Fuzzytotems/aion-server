#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is displaying visible players.
 *
 * @author -Nemesiss-, Avol, srx47, cura, -Enomine-, -Artur-, Neon
 */
class SM_PLAYER_INFO : public AbstractPlayerInfoPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	bool enemy{};
public:
	explicit SM_PLAYER_INFO(model::gameobjects::player::Player& player);
	SM_PLAYER_INFO(model::gameobjects::player::Player& player, bool enemy);
	~SM_PLAYER_INFO() override;

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
