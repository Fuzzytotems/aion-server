#include "aion/gameserver/services/instance/InstanceScaler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/InstanceCooltime.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::instance {

namespace {

using configs::main::InstanceConfig;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;

/** Java: instance.getPlayersInside().stream().filter(p -> !p.isStaff()) */
std::vector<runtime::Ptr<Player>> nonStaffPlayersInside(world::WorldMapInstance& instance) {
	std::vector<runtime::Ptr<Player>> players;
	for (const runtime::Ptr<Player>& p : instance.getPlayersInside()) {
		if (!p->isStaff())
			players.push_back(p);
	}
	return players;
}

} // namespace

runtime::HashMap<runtime::Ref<world::WorldMapInstance>, runtime::Ref<InstanceScaler::Scaling>> InstanceScaler::scalings{
	AION_LOCK_CLASS(InstanceScaler::scalings)};

InstanceScaler::InstanceScaler() = default;

InstanceScaler::~InstanceScaler() = default;

InstanceScaler& InstanceScaler::getInstance() {
	static InstanceScaler instance; // Java: INSTANCE
	return instance;
}

runtime::Ref<InstanceScaler::Scaling> InstanceScaler::Scaling::create() {
	return runtime::makeRef<Scaling>();
}

bool InstanceScaler::Scaling::update(world::WorldMapInstance& instance) {
	std::vector<runtime::Ptr<Player>> players = nonStaffPlayersInside(instance);
	int32_t newPlayerCount = static_cast<int32_t>(players.size());
	if (newPlayerCount < instance.getMaxPlayers() && isLowLevelInstanceWithHighLevelPlayers(instance, players))
		newPlayerCount = instance.getMaxPlayers(); // disable scaling
	if (playerCount.get() >= newPlayerCount)
		return false;
	playerCount.set(newPlayerCount);
	runtime::Ref<runtime::RcArrayList<runtime::Ref<InstanceScalerStatFunction>>> functions =
		runtime::RcArrayList<runtime::Ref<InstanceScalerStatFunction>>::create();
	for (runtime::Ref<InstanceScalerStatFunction>& function : createStatFunctions(instance, newPlayerCount))
		functions->add(std::move(function));
	statFunctions.set(std::move(functions));
	return true;
}

bool InstanceScaler::Scaling::isLowLevelInstanceWithHighLevelPlayers(world::WorldMapInstance& instance,
	const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	if (players.empty())
		return false;
	int32_t maxAllowedLevel = getInstanceEnterMinLevel(instance, players) + InstanceConfig::INSTANCE_SCALING_MAX_LEVEL_DIFF.load();
	int32_t maxLevel = players.front()->getLevel(); // Java: mapToInt(Player::getLevel).max().orElseThrow() (players is not empty)
	for (const runtime::Ptr<Player>& player : players)
		maxLevel = std::max<int32_t>(maxLevel, player->getLevel());
	return maxLevel > maxAllowedLevel;
}

int32_t InstanceScaler::Scaling::getInstanceEnterMinLevel(world::WorldMapInstance& instance,
	const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	const model::templates::InstanceCooltime* ct = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getInstanceCooltimeByWorldId(instance.getMapId());
	if (players.empty())
		return 1; // Java: min().orElse(1)
	if (ct == nullptr)
		throw runtime::NullPointerException("INSTANCE_COOLTIME_DATA.getInstanceCooltimeByWorldId(" + std::to_string(instance.getMapId()) + ")");
	int32_t minLevel = 0;
	bool first = true;
	for (const runtime::Ptr<Player>& p : players) {
		int32_t level = p->getRace() == model::Race::ASMODIANS ? ct->getEnterMinLevelDark() : ct->getEnterMinLevelLight();
		minLevel = first ? level : std::min(minLevel, level);
		first = false;
	}
	return minLevel;
}

std::vector<runtime::Ref<InstanceScaler::InstanceScalerStatFunction>> InstanceScaler::Scaling::createStatFunctions(world::WorldMapInstance& instance,
	int32_t value) {
	using model::stats::container::StatEnum;
	std::vector<runtime::Ref<InstanceScalerStatFunction>> functions;
	float hpMulti =
		calculateMultiplier(instance, InstanceConfig::INSTANCE_SCALING_HP_FLOOR.load(), InstanceConfig::INSTANCE_SCALING_HP_SCALE_FACTOR.load(), value);
	float dmgMulti =
		calculateMultiplier(instance, InstanceConfig::INSTANCE_SCALING_DMG_FLOOR.load(), InstanceConfig::INSTANCE_SCALING_DMG_SCALE_FACTOR.load(), value);
	if (hpMulti != 1) {
		functions.push_back(InstanceScalerStatFunction::create(StatEnum::MAXHP, hpMulti));
	}
	if (dmgMulti != 1) {
		functions.push_back(InstanceScalerStatFunction::create(StatEnum::PHYSICAL_ATTACK, dmgMulti));
		functions.push_back(InstanceScalerStatFunction::create(StatEnum::MAGICAL_ATTACK, dmgMulti));
		functions.push_back(InstanceScalerStatFunction::create(StatEnum::BOOST_SPELL_ATTACK, dmgMulti));
	}
	return functions;
}

InstanceScaler::Scaling::~Scaling() = default;

InstanceScaler::InstanceScalerStatFunction::InstanceScalerStatFunction(model::stats::container::StatEnum statValue, float rateValue) : rate(rateValue) {
	this->stat = statValue;
}

runtime::Ref<InstanceScaler::InstanceScalerStatFunction> InstanceScaler::InstanceScalerStatFunction::create(model::stats::container::StatEnum statValue,
	float rateValue) {
	return runtime::makeRef<InstanceScaler::InstanceScalerStatFunction>(statValue, rateValue);
}

void InstanceScaler::InstanceScalerStatFunction::apply(model::stats::calc::Stat2& statValue,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	statValue.setBaseRate(statValue.getBaseRate() * rate);
	statValue.setBonusRate(statValue.getBonusRate() * rate);
}

int32_t InstanceScaler::InstanceScalerStatFunction::getPriority() const {
	return 120;
}

InstanceScaler::InstanceScalerStatFunction::~InstanceScalerStatFunction() = default;

void InstanceScaler::onEnterInstance(model::gameobjects::player::Player& player) {
	runtime::Ptr<world::WorldMapInstance> instance = player.getPosition()->getWorldMapInstance();
	if (!canScale(*instance))
		return;
	runtime::Ptr<Scaling> scaling =
		scalings.computeIfAbsent(runtime::Ref<world::WorldMapInstance>(instance), [] { return Scaling::create(); });
	SYNCHRONIZED(*scaling) {
		if (scaling->update(*instance))
			rescale(*instance, *scaling);
	}
}

void InstanceScaler::onBeforeSpawn(model::gameobjects::Npc& npc) {
	runtime::Ptr<world::WorldMapInstance> instance = npc.getPosition()->getWorldMapInstance();
	if (!canScale(*instance))
		return;
	runtime::Ptr<Scaling> scaling = scalings.get(instance);
	if (!scaling)
		return;
	SYNCHRONIZED(*scaling) {
		if (shouldScale(npc, *instance))
			scaleNpc(npc, *scaling);
	}
}

void InstanceScaler::rescale(world::WorldMapInstance& instance, InstanceScaler::Scaling& scaling) {
	for (const runtime::Ptr<Npc>& npc : instance.getNpcs())
		if (shouldScale(*npc, instance))
			scaleNpc(*npc, scaling);
}

bool InstanceScaler::canScale(world::WorldMapInstance& instance) {
	if (!InstanceConfig::INSTANCE_SCALING_ENABLE.load() || instance.getMaxPlayers() <= 1 || !instance.getParent()->isInstanceType())
		return false;
	std::shared_ptr<const std::unordered_set<int32_t>> excludedMaps = InstanceConfig::INSTANCE_SCALING_EXCLUDED_MAPS.get();
	return !(excludedMaps && excludedMaps->contains(instance.getMapId()));
}

bool InstanceScaler::shouldScale(model::gameobjects::Npc& npc, world::WorldMapInstance& instance) {
	// Java: npc.getRating().ordinal() >= INSTANCE_SCALING_NPC_MIN_RATING.ordinal() (the config enum has NpcRating's constants in the same order)
	if (static_cast<int32_t>(npc.getRating()) < static_cast<int32_t>(InstanceConfig::INSTANCE_SCALING_NPC_MIN_RATING.load()) || npc.isDead())
		return false;
	// Java: getPlayersInside().stream().filter(p -> !p.isStaff()).findFirst().map(npc::isEnemyFrom).orElse(false)
	for (const runtime::Ptr<Player>& p : instance.getPlayersInside()) {
		if (!p->isStaff())
			return npc.isEnemyFrom(*p);
	}
	return false;
}

void InstanceScaler::scaleNpc(model::gameobjects::Npc& npc, InstanceScaler::Scaling& scaling) {
	npc.getGameStats()->endEffect(getInstance());
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<InstanceScalerStatFunction>>> statFunctions = scaling.statFunctions.get();
	if (statFunctions && !statFunctions->isEmpty()) { // Java: Collections.emptyList() before the first update (C++: null)
		std::vector<runtime::Ptr<model::stats::calc::functions::IStatFunction>> functions;
		for (const runtime::Ptr<InstanceScalerStatFunction>& function : statFunctions->snapshot())
			functions.emplace_back(function);
		npc.getGameStats()->addEffect(runtime::Ptr<model::stats::calc::StatOwner>(getInstance()), functions);
	}
}

float InstanceScaler::calculateMultiplier(world::WorldMapInstance& instance, float floor, float scaleFactor, int32_t playerCount) {
	float multi = static_cast<float>(std::min(playerCount, instance.getMaxPlayers())) / static_cast<float>(instance.getMaxPlayers());
	multi = 1 - (1 - multi) * scaleFactor;
	// Java: Math.max(floor, multi) (NaN if either is NaN)
	if (std::isnan(floor) || std::isnan(multi))
		return std::numeric_limits<float>::quiet_NaN();
	return std::max(floor, multi);
}

} // namespace aion::gameserver::services::instance
