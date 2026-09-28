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
class SM_MANTRA_EFFECT : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> effector{};
	int32_t subEffectId{};
public:
	SM_MANTRA_EFFECT(model::gameobjects::Creature& effector, int32_t subEffectId);
	~SM_MANTRA_EFFECT() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
