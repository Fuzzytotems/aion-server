#include "aion/gameserver/questEngine/handlers/models/KillSpawnedData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/template/KillSpawned.h"

namespace aion::gameserver::questEngine::handlers::models {

void KillSpawnedData::register_(QuestEngine& questEngine) const {
	// the handler keeps pointers to this data's Monster elements (Java: the same objects)
	questEngine.addQuestHandler(std::make_unique<template_::KillSpawned>(id, startNpcIds, endNpcIds, monster));
}

std::optional<std::unordered_set<int32_t>> KillSpawnedData::getAlternativeNpcs(int32_t npcId) const {
	for (const Monster& m : monster) {
		if (auto others = otherNpcIds(m.getNpcIds(), npcId))
			return others;
	}
	return MonsterHuntData::getAlternativeNpcs(npcId);
}

} // namespace aion::gameserver::questEngine::handlers::models
