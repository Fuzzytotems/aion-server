#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xavier
 */
class SM_DUEL : public AionServerPacket {
private:
	std::string playerName{};
	model::DuelResult result{};
	int32_t requesterObjId{};
	int32_t type{};
	explicit SM_DUEL(int32_t type);

public:
	static SM_DUEL SM_DUEL_STARTED(int32_t requesterObjId);

private:
	void setRequesterObjId(int32_t value) { this->requesterObjId = value; }

public:
	static SM_DUEL SM_DUEL_RESULT(model::DuelResult result, std::string_view playerName);

private:
	void setPlayerName(std::string_view value) { this->playerName = value; }
	void setResult(model::DuelResult value) { this->result = value; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
