#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr
 */
class SM_SKILL_CANCEL : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> creature{};
	int32_t skillId{};
public:
	SM_SKILL_CANCEL(model::gameobjects::Creature& creature, int32_t skillId);
	~SM_SKILL_CANCEL() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
