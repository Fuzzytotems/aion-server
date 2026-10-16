#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/gather/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet updates the players current gathering status / progress.
 *
 * @author ATracer, orz, Yeats, Neon
 */
class SM_GATHER_UPDATE : public AionServerPacket {
private:
	int32_t skillId{};
	int32_t action{};
	int32_t itemId{};
	int32_t success{};
	int32_t failure{};
	std::string l10n{};
	int32_t executionSpeed{};
	int32_t delay{};

public:
	SM_GATHER_UPDATE(const model::templates::gather::GatherableTemplate* template_, const model::templates::gather::Material* material,
		int32_t success, int32_t failure, int32_t action, int32_t executionSpeed, int32_t delay);

protected:
	void writeImpl(AionConnection* con) override;

private:
	void writeSystemMsgInfo(int32_t msgId);
};

} // namespace aion::gameserver::network::aion::serverpackets
