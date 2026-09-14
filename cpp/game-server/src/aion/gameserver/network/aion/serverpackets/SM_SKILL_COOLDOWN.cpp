#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SKILL_COOLDOWN::Cooldown::Cooldown(int32_t skillIdValue, int64_t expirationTimeMillisValue)
	: skillId_(skillIdValue), expirationTimeMillis_(expirationTimeMillisValue) {
}

SM_SKILL_COOLDOWN::Cooldown::~Cooldown() = default;

runtime::Ref<SM_SKILL_COOLDOWN::Cooldown> SM_SKILL_COOLDOWN::Cooldown::create(int32_t skillId, int64_t expirationTimeMillis) {
	return runtime::makeRef<Cooldown>(skillId, expirationTimeMillis);
}

int32_t SM_SKILL_COOLDOWN::Cooldown::getRemainingSeconds() {
	AION_UNPORTED();
}

int32_t SM_SKILL_COOLDOWN::Cooldown::getDurationMillis() {
	AION_UNPORTED();
}

bool SM_SKILL_COOLDOWN::Cooldown::equals(const Cooldown& obj) const {
	return this == &obj || (skillId_ == obj.skillId_ && expirationTimeMillis_ == obj.expirationTimeMillis_);
}

int32_t SM_SKILL_COOLDOWN::Cooldown::hashCode() const {
	// Java record hashCode (java.lang.runtime.ObjectMethods): 31 * h + hash(component); Long.hashCode is (int) (value ^ (value >>> 32))
	const auto bits = static_cast<uint64_t>(expirationTimeMillis_);
	uint32_t h = static_cast<uint32_t>(skillId_);
	h = 31 * h + static_cast<uint32_t>(bits ^ (bits >> 32));
	return static_cast<int32_t>(h);
}

SM_SKILL_COOLDOWN::SM_SKILL_COOLDOWN(int32_t skillId, int64_t expirationTimeMillis)
	: AionServerPacket(opcodeOf<SM_SKILL_COOLDOWN>), notify(true) {
	cooldowns.push_back(Cooldown::create(skillId, expirationTimeMillis));
}

SM_SKILL_COOLDOWN::SM_SKILL_COOLDOWN(model::gameobjects::player::Player& player,
	const std::unordered_map<int32_t, int64_t>& cooldownExpirationMillisByCooldownId, bool notifyValue)
	: AionServerPacket(opcodeOf<SM_SKILL_COOLDOWN>), notify(notifyValue) {
	AION_UNPORTED();
}

SM_SKILL_COOLDOWN::SM_SKILL_COOLDOWN(model::gameobjects::player::Player& player, const std::vector<int32_t>& resettableCooldownIds)
	: AionServerPacket(opcodeOf<SM_SKILL_COOLDOWN>) {
	AION_UNPORTED();
}

SM_SKILL_COOLDOWN::~SM_SKILL_COOLDOWN() = default;

void SM_SKILL_COOLDOWN::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
