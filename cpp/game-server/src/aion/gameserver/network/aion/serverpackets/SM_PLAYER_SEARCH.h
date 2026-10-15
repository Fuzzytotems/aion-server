#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Sent to fill the search panel of a players social window<br />
 * I.E.: In response to a <tt>CM_PLAYER_SEARCH</tt>
 *
 * @author Ben
 */
class SM_PLAYER_SEARCH : public AionServerPacket {
private:
	std::vector<runtime::Ref<model::gameobjects::player::Player>> players{};
public:
	/** Constructs a new packet that will send these players */
	explicit SM_PLAYER_SEARCH(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players);
	~SM_PLAYER_SEARCH() override;

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
