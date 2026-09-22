#include "aion/gameserver/controllers/attack/DamageList.h"

#include <algorithm>
#include <cstdint>

#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers::attack {

using runtime::Ptr;

namespace {

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

DamageList::DamageList(const std::vector<runtime::Ptr<AggroInfo>>& aggroInfos, model::gameobjects::Creature& owner) {
	for (const Ptr<AggroInfo>& aggroInfo : aggroInfos) {
		if (aggroInfo->getDamage() <= 0)
			continue;
		// Java: aggroInfo.getAttacker().getMaster() - Creature.getMaster() returns `this` by default, so it is never null
		Ptr<model::gameobjects::Creature> attackerMaster = aggroInfo->getAttacker()->getMaster();
		if (!attackerMaster)
			throw runtime::NullPointerException("attackerMaster is null");
		// Don't include damage from creatures outside the known list.
		if (!owner.getKnownList().knows(*attackerMaster))
			continue;
		// Java: damageByCreature.computeIfAbsent(attackerMaster, DamageInfo::new).addDamage(...)
		auto entry = damageByCreature.try_emplace(attackerMaster, *attackerMaster).first;
		entry->second.addDamage(aggroInfo->getDamage());
	}
}

TeamDamageList DamageList::toTeamDamages() {
	return TeamDamageList(*this);
}

std::vector<DamageInfo> DamageList::getCreatureDamages() {
	std::vector<DamageInfo> damages;
	damages.reserve(damageByCreature.size());
	for (const auto& [creature, damageInfo] : damageByCreature)
		damages.push_back(damageInfo);
	return damages;
}

std::optional<DamageInfo> DamageList::getMostDamage() {
	// Java: values().stream().max(Comparator.comparingInt(DamageInfo::getDamage)) - the first maximum in encounter order (Stream.max keeps the
	// earlier of two equal elements), and a HashMap's encounter order is unspecified in Java as in C++
	std::optional<DamageInfo> most;
	for (const auto& [creature, damageInfo] : damageByCreature) {
		if (!most || damageInfo.getDamage() > most->getDamage())
			most = damageInfo;
	}
	return most;
}

int32_t DamageList::getTotalDamage() {
	// Java: mapToInt(DamageInfo::getDamage).sum() - an int sum, which wraps on overflow
	int32_t total = 0;
	for (const auto& [creature, damageInfo] : damageByCreature)
		total = addInt(total, damageInfo.getDamage());
	return total;
}

} // namespace aion::gameserver::controllers::attack
