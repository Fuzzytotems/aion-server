#pragma once

#include <cstdint>

#include "aion/gameserver/skillengine/effect/SubEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SubEffect (a behaviour class since upstream 6c6eee658 added chance_delta). @author ATracer */
class SubEffect : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/SubEffect.xml.inc"
public:
	/** Java int arithmetic (wraps) */
	int32_t getChance(int32_t skillLevel) const {
		return static_cast<int32_t>(static_cast<uint32_t>(chance) + static_cast<uint32_t>(chanceDelta) * static_cast<uint32_t>(skillLevel));
	}
};

} // namespace aion::gameserver::skillengine::effect
