#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"

#include <algorithm>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/**
 * Java: resettableCooldownIds.stream().collect(Collectors.toMap(c -> c, _ -> 0L)), which throws IllegalStateException for a duplicate id
 */
std::unordered_map<int32_t, int64_t> resetCooldowns(const std::vector<int32_t>& resettableCooldownIds) {
	std::unordered_map<int32_t, int64_t> cooldowns;
	for (int32_t cooldownId : resettableCooldownIds) {
		if (!cooldowns.emplace(cooldownId, 0).second)
			throw commons::utils::IllegalStateException("Duplicate key " + std::to_string(cooldownId) + " (attempted merging values 0 and 0)");
	}
	return cooldowns;
}

} // namespace

SM_SKILL_COOLDOWN::Cooldown::Cooldown(int32_t skillIdValue, int64_t expirationTimeMillisValue)
	: skillId_(skillIdValue), expirationTimeMillis_(expirationTimeMillisValue) {
}

SM_SKILL_COOLDOWN::Cooldown::~Cooldown() = default;

runtime::Ref<SM_SKILL_COOLDOWN::Cooldown> SM_SKILL_COOLDOWN::Cooldown::create(int32_t skillId, int64_t expirationTimeMillis) {
	return runtime::makeRef<Cooldown>(skillId, expirationTimeMillis);
}

int32_t SM_SKILL_COOLDOWN::Cooldown::getRemainingSeconds() {
	return expirationTimeMillis_ == 0 ? 0 : static_cast<int32_t>(std::max<int64_t>(0, (expirationTimeMillis_ - commons::utils::currentTimeMillis()) / 1000));
}

int32_t SM_SKILL_COOLDOWN::Cooldown::getDurationMillis() {
	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId_);
	if (skillTemplate == nullptr) // Java: DataManager.SKILL_DATA.getSkillTemplate(skillId).getCooldown() on null
		throw runtime::NullPointerException("SM_SKILL_COOLDOWN: no skill template " + std::to_string(skillId_));
	return skillTemplate->getCooldown() * 100;
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
	for (const runtime::Ptr<model::skill::PlayerSkillEntry>& skill : player.getSkillList()->getAllSkills()) {
		auto cooldownExpirationMillis = cooldownExpirationMillisByCooldownId.find(skill->getSkillTemplate()->getCooldownId());
		if (cooldownExpirationMillis != cooldownExpirationMillisByCooldownId.end())
			cooldowns.push_back(Cooldown::create(skill->getSkillId(), cooldownExpirationMillis->second));
	}
	// The game plays the same icon cooldown animation for all skills that share a cooldownId and the last entry per cooldownId wins, so we sort by
	// animation duration to avoid animations that are shorter than the remaining cooldown time.
	// Java List.sort is stable; Comparator.comparingInt evaluates getDurationMillis for each comparison
	std::ranges::stable_sort(cooldowns, [](const runtime::Ref<Cooldown>& a, const runtime::Ref<Cooldown>& b) {
		return a->getDurationMillis() < b->getDurationMillis();
	});
}

SM_SKILL_COOLDOWN::SM_SKILL_COOLDOWN(model::gameobjects::player::Player& player, const std::vector<int32_t>& resettableCooldownIds)
	: SM_SKILL_COOLDOWN(player, resetCooldowns(resettableCooldownIds), true) {
}

SM_SKILL_COOLDOWN::~SM_SKILL_COOLDOWN() = default;

void SM_SKILL_COOLDOWN::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(cooldowns.size()));
	writeC(notify ? 1 : 0); // 1 will trigger a notification sound and animation on all sent skills
	for (const runtime::Ref<Cooldown>& cooldown : cooldowns) {
		writeH(cooldown->skillId());
		writeD(cooldown->getRemainingSeconds());
		writeD(cooldown->getDurationMillis()); // 0 also seems to always work
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
