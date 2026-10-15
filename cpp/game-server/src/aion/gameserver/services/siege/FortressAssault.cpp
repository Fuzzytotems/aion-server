#include "aion/gameserver/services/siege/FortressAssault.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/Assaulter.h"
#include "aion/gameserver/model/siege/AssaulterType.h"
#include "aion/gameserver/model/siege/AssaulterTypeInfo.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/siegelocation/AssaultData.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_ASSEMBLER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/siege/BalaurAssaultService.h"
#include "aion/gameserver/services/siege/FortressSiege.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::siege {

using model::siege::Assaulter;
using model::siege::AssaulterType;
using model::siege::SiegeRace;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

namespace {
/** Java: assaultData.getProcessedAssaulters().get(type) - the EnumMap's list, null (nullptr) for a type without assaulters */
const std::vector<runtime::Ref<Assaulter>>* assaultersOf(const model::templates::siegelocation::AssaultData& data, AssaulterType type) {
	const std::optional<std::vector<runtime::Ref<Assaulter>>>& list = data.getProcessedAssaulters()[static_cast<size_t>(type)];
	return list ? &*list : nullptr;
}

std::string javaFloat(float value) {
	return geoEngine::math::JavaFloat::toString(value);
}
} // namespace

// Java FortressAssault.java:39-43
FortressAssault::FortressAssault(FortressSiege& siege) : Assault(siege), assaultData(siegeLocation->getTemplate()->getAssaultData()) {
	calculateDifficultySettings();
}

runtime::Ref<FortressAssault> FortressAssault::create(FortressSiege& siege) {
	return runtime::makeRef<FortressAssault>(siege);
}

// Java FortressAssault.java:45-49
void FortressAssault::handleAssault() {
	BalaurAssaultService::getInstance().spawnDredgion(assaultData->getDredgionId());
	scheduleSpawns();
}

// Java FortressAssault.java:51-55
void FortressAssault::onAssaultFinish(bool isCaptured) {
	if (isCaptured)
		announce(SM_SYSTEM_MESSAGE::STR_ABYSS_DRAGON_BOSS_KILLED(getBossNpcL10n()));
}

// Java FortressAssault.java:57-60; lambda at FortressAssault.java:59 (fieldmap key siege.FortressAssault@L59:56): the method reference this::spawnWave
void FortressAssault::scheduleSpawns() {
	const int32_t delay = minSpawnDelay.get() >= assaultData->getBaseDelay() ? minSpawnDelay.get()
	                                                                        : commons::utils::Rnd::get(minSpawnDelay.get(), assaultData->getBaseDelay());
	spawnTask.set(utils::ThreadPoolManager::getInstance().schedule(this, &FortressAssault::spawnWave, static_cast<int64_t>(delay) * 1000));
}

// Java FortressAssault.java:62-90
void FortressAssault::spawnWave() {
	if (!siegeLocation->isVulnerable() || spawnBudget.get() < 0.1f)
		return;

	waveCount.set(waveCount.get() + 1);
	switch (waveCount.get()) {
		case 1:
		case 10: {
			const std::vector<runtime::Ref<Assaulter>>* teleportWave = assaultersOf(*assaultData, AssaulterType::TELEPORT);
			for (const runtime::Ptr<model::gameobjects::siege::SiegeNpc>& npc : world::World::getInstance().getLocalSiegeNpcs(locationId))
				if (npc->getRating() != model::templates::npc::NpcRating::LEGENDARY &&
					npc->getAbyssNpcType() != model::templates::npc::AbyssNpcType::ARTIFACT && commons::utils::Rnd::chance() < 40) {
					// Java: Rnd.get(teleportWave) - null for a missing or empty list, then spawnAssaulter's NullPointerException
					const runtime::Ref<Assaulter>* a = teleportWave != nullptr ? commons::utils::Rnd::get(*teleportWave) : nullptr;
					if (a == nullptr)
						throw runtime::NullPointerException("Assaulter");
					spawnAssaulter(**a, *npc);
				}
			announce(SM_SYSTEM_MESSAGE::STR_ABYSS_WARP_DRAGON());
			break;
		}
		default:
			for (const runtime::Ref<Assaulter>& a : computeWave())
				spawnAssaulter(*a, *boss);
			if (!commanderSpawnList.isEmpty()) {
				if (commons::utils::Rnd::chance() < commanderSpawnChance.get()) {
					spawnAssaulter(*commanderSpawnList.removeAt(0), *boss);
					commanderSpawnChance.set(0.0f);
				} else {
					commanderSpawnChance.set(commanderSpawnChance.get() + (15 + 5 * difficulty.get()));
				}
			}
			announce(SM_SYSTEM_MESSAGE::STR_ABYSS_CARRIER_DROP_DRAGON());
			break;
	}
	scheduleSpawns();
}

// Java FortressAssault.java:92-101
std::vector<runtime::Ref<Assaulter>> FortressAssault::computeWave() {
	std::vector<runtime::Ref<Assaulter>> finalList;
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<AssaulterType>::names.size(); ordinal++) {
		const AssaulterType type = static_cast<AssaulterType>(ordinal);
		if (type == AssaulterType::TELEPORT || type == AssaulterType::COMMANDER)
			continue;
		addAssaulters(finalList, assaultersOf(*assaultData, type), spawnBudget.get() * getSpawnStake(type));
	}
	return finalList;
}

// Java FortressAssault.java:103-115
void FortressAssault::addAssaulters(std::vector<runtime::Ref<Assaulter>>& output, const std::vector<runtime::Ref<Assaulter>>* input, float budget) {
	if (input == nullptr || input->empty())
		return;
	while (budget > 0.0f) {
		const float budgetCopy = budget;
		std::vector<runtime::Ref<Assaulter>> affordable;
		for (const runtime::Ref<Assaulter>& assaulter : *input)
			if (assaulter->getSpawnCost() <= budgetCopy)
				affordable.push_back(assaulter);
		std::optional<runtime::Ref<Assaulter>> a = commons::utils::Rnd::get(std::move(affordable));
		if (!a) {
			a = input->front();
		}
		output.push_back(*a);
		budget -= (*a)->getSpawnCost();
	}
}

// Java FortressAssault.java:117-119
void FortressAssault::announce(SM_SYSTEM_MESSAGE msg) {
	siegeLocation->forEachPlayer([&msg](model::gameobjects::player::Player& p) { utils::PacketSendUtility::sendPacket(p, msg); });
}

// Java FortressAssault.java:121-139
void FortressAssault::calculateDifficultySettings() {
	const float factionBalance = getFactionBalanceMultiplier();
	const float influence = getInfluenceMultiplier();

	difficulty.set(factionBalance / 3.0f * (1.0f + influence) * configs::main::SiegeConfig::SIEGE_DIFFICULTY_MULTIPLIER.load());

	if (assaultData == nullptr) // Java: the NullPointerException of assaultData.getBaseBudget() for a fortress without <assault_data>
		throw runtime::NullPointerException("AssaultData");
	// Java: Math.max(float, int) - the int rounded budget widened to float
	spawnBudget.set(std::max(static_cast<float>(assaultData->getBaseBudget()) / 3.0f,
		static_cast<float>(utils::JavaMath::round(static_cast<float>(assaultData->getBaseBudget()) * difficulty.get()))));
	startBudget.set(spawnBudget.get());
	// Java adds into commanderSpawnList directly; the C++ addAssaulters fills a vector, appended in the same order
	std::vector<runtime::Ref<Assaulter>> commanders;
	addAssaulters(commanders, assaultersOf(*assaultData, AssaulterType::COMMANDER), difficulty.get());
	commanderSpawnList.addAll(commanders);
	possibleCommanderCount.set(commanderSpawnList.size());

	minSpawnDelay.set(std::min(utils::JavaMath::round(static_cast<float>(assaultData->getBaseDelay()) / difficulty.get()), assaultData->getBaseDelay() - 10));
	if (minSpawnDelay.get() < 30) // just in case SIEGE_DIFFICULTY_MULTIPLIER is set beyond 1.0 (100%)
		minSpawnDelay.set(30);

	log.info("Initialized fortress assault on [locationID=" + std::to_string(locationId) + "] with [difficulty=" + javaFloat(difficulty.get()) +
		"] [factionBalance=" + javaFloat(factionBalance) + "] [influence=" + javaFloat(influence) + "] [difficultyMultiplier=" +
		javaFloat(configs::main::SiegeConfig::SIEGE_DIFFICULTY_MULTIPLIER.load()) + "]");
}

// Java FortressAssault.java:141-154
float FortressAssault::getFactionBalanceMultiplier() {
	const int32_t factionBalance = siegeLocation->getFactionBalance();
	switch (siegeLocation->getRace()) {
		case SiegeRace::ASMODIANS:
			if (factionBalance < 0)
				return static_cast<float>(std::abs(factionBalance));
			break;
		case SiegeRace::ELYOS:
			if (factionBalance > 0)
				return static_cast<float>(std::abs(factionBalance));
			break;
		default:
			break;
	}
	return 1.0f;
}

// Java FortressAssault.java:156-165
float FortressAssault::getInfluenceMultiplier() {
	switch (siegeLocation->getRace()) {
		case SiegeRace::ASMODIANS:
			return model::siege::Influence::getInstance().getAsmodianInfluenceRate();
		case SiegeRace::ELYOS:
			return model::siege::Influence::getInstance().getElyosInfluenceRate();
		default:
			return 1.0f;
	}
}

// Java FortressAssault.java:167-177
void FortressAssault::onDredgionCommanderKilled() {
	spawnBudget.set(spawnBudget.get() - startBudget.get() * (1.0f / static_cast<float>(possibleCommanderCount.get())));
	if (spawnBudget.get() < 0.1f) {
		world::World::getInstance().forEachPlayer([](model::gameobjects::player::Player& p) {
			utils::PacketSendUtility::sendPacket(p, network::aion::serverpackets::SM_NPC_ASSEMBLER(nullptr));
			utils::PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE::STR_ABYSS_CARRIER_DESPAWN());
		});
		if (runtime::FutureRef task = spawnTask.get())
			task->cancel(true);
		log.info("Finished fortress assault on [locationID=" + std::to_string(locationId) + "] by defeating " +
			std::to_string(possibleCommanderCount.get()) + " dredgion commanders after " + std::to_string(waveCount.get()) + " waves.");
	}
}

FortressAssault::~FortressAssault() = default;

} // namespace aion::gameserver::services::siege
