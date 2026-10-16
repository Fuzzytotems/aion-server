#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet show casting spell animation.
 *
 * @author alexa026, rhys2002
 */
class SM_CASTSPELL : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> effector{};
	int32_t spellId{};
	int32_t level{};
	int32_t targetType{};
	int32_t targetObjectId{};
	int32_t castDuration{};
	float castSpeed{};
	bool allowAnimationBoostByCastSpeed{};
	float x{};
	float y{};
	float z{};

public:
	SM_CASTSPELL(model::gameobjects::Creature& effector, int32_t spellId, int32_t level, int32_t targetType, int32_t targetObjectId,
		int32_t castDuration, float castSpeed, bool allowAnimationBoostByCastSpeed);
	SM_CASTSPELL(model::gameobjects::Creature& effector, int32_t spellId, int32_t level, int32_t targetType, float x, float y, float z,
		int32_t castDuration, float castSpeed, bool allowAnimationBoostByCastSpeed);
	~SM_CASTSPELL() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
