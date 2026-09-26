#include "aion/gameserver/skillengine/effect/SkillCooltimeResetEffect.h"

#include <cstdint>
#include <unordered_map>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

namespace {

/** Java long a - b (wraps on overflow) */
constexpr int64_t subLong(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b));
}

/** Java long a + b (wraps on overflow) */
constexpr int64_t addLong(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

/** Java long a * b (wraps on overflow) */
constexpr int64_t mulLong(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

} // namespace

void SkillCooltimeResetEffect::applyEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	std::unordered_map<int32_t, int64_t> resetSkillCoolDowns;
	// Java `i++` on an int (wraps, as Java's loop would for a last_cd of Integer.MAX_VALUE, which the data has not)
	for (int32_t i = firstCd; i <= lastCd; i = static_cast<int32_t>(static_cast<uint32_t>(i) + 1u)) {
		int64_t delay = subLong(effected->getSkillCoolDown(i), commons::utils::currentTimeMillis());
		if (delay <= 0)
			continue;
		if (delta > 0) // TODO: Percent of remaining CD or original cd?
			delay = subLong(delay, mulLong(delay, delta / 100)); // Java int division: a delta below 100 takes nothing off
		else
			delay = subLong(delay, value);
		effected->setSkillCoolDown(i, addLong(delay, commons::utils::currentTimeMillis()));
		resetSkillCoolDowns.insert_or_assign(i, addLong(delay, commons::utils::currentTimeMillis()));
	}
	if (!resetSkillCoolDowns.empty()) {
		if (Ptr<Player> player = runtime::as<Player>(effected))
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SKILL_COOLDOWN(*player, resetSkillCoolDowns, true));
	}
}

} // namespace aion::gameserver::skillengine::effect
