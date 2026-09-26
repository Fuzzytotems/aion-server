#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz, Neon
 */
class SM_SKILL_REMOVE : public AionServerPacket {
private:
	int32_t skillId{};
	int32_t skillLevel{};
	int32_t skillType{};
public:
	explicit SM_SKILL_REMOVE(model::skill::PlayerSkillEntry& skill);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
