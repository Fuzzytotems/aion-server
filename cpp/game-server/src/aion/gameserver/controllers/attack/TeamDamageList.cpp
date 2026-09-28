#include "aion/gameserver/controllers/attack/TeamDamageList.h"

#include <cstdint>

#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"

namespace aion::gameserver::controllers::attack {

using runtime::Ptr;

namespace {

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

// callbacks: the compute/computeIfAbsent lambdas of the Java constructor run during the call
TeamDamageList::TeamDamageList(DamageList& damageList) {
	for (const DamageInfo& damageInfo : damageList.getCreatureDamages()) {
		Ptr<model::gameobjects::AionObject> creatureOrTeam = damageInfo.getAttacker();
		Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(creatureOrTeam);
		Ptr<model::team::TemporaryPlayerTeam> team = player ? player->getCurrentTeam() : Ptr<model::team::TemporaryPlayerTeam>();
		if (team) {
			creatureOrTeam = team;
			// Java: the member's own DamageInfo is stored (the cast DamageInfo<Player> is erased away here), not a new one keyed by the team
			const DamageInfo& memberDamage = damageInfo;
			auto entry = mostDamageByTeam.find(team);
			if (entry == mostDamageByTeam.end())
				mostDamageByTeam.emplace(team, memberDamage);
			else if (memberDamage.getDamage() > entry->second.getDamage())
				entry->second = memberDamage;
		}
		auto entry = damageByCreatureOrTeam.try_emplace(creatureOrTeam, *creatureOrTeam).first;
		entry->second.addDamage(damageInfo.getDamage());
	}
}

std::vector<DamageInfo> TeamDamageList::getCreatureOrTeamDamages() {
	std::vector<DamageInfo> damages;
	damages.reserve(damageByCreatureOrTeam.size());
	for (const auto& [creatureOrTeam, damageInfo] : damageByCreatureOrTeam)
		damages.push_back(damageInfo);
	return damages;
}

std::optional<DamageInfo> TeamDamageList::getMostDamage() {
	// Java: values().stream().max(Comparator.comparingInt(DamageInfo::getDamage)) - the first maximum in encounter order (see DamageList)
	std::optional<DamageInfo> most;
	for (const auto& [creatureOrTeam, damageInfo] : damageByCreatureOrTeam) {
		if (!most || damageInfo.getDamage() > most->getDamage())
			most = damageInfo;
	}
	return most;
}

std::optional<DamageInfo> TeamDamageList::getMostDamageByTeam(model::team::TemporaryPlayerTeam& team) {
	auto entry = mostDamageByTeam.find(Ptr<model::team::TemporaryPlayerTeam>(team));
	if (entry == mostDamageByTeam.end())
		return std::nullopt;
	return entry->second;
}

int32_t TeamDamageList::getTotalDamage() {
	// Java: mapToInt(DamageInfo::getDamage).sum() - an int sum, which wraps on overflow
	int32_t total = 0;
	for (const auto& [creatureOrTeam, damageInfo] : damageByCreatureOrTeam)
		total = addInt(total, damageInfo.getDamage());
	return total;
}

} // namespace aion::gameserver::controllers::attack
