#include "aion/gameserver/questEngine/handlers/models/KillInWorldData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/KillInWorld.h"

namespace aion::gameserver::questEngine::handlers::models {

void KillInWorldData::register_(QuestEngine& questEngine) const {
	questEngine.addQuestHandler(std::make_unique<template_::KillInWorld>(id, endNpcIds, startNpcIds, worldIds, amount, minRank, levelDiff, invasionWorld,
		startDialogId, startDistanceNpcId, endDialogId));
}

std::optional<std::unordered_set<int32_t>> KillInWorldData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	if (auto others = otherNpcIds(endNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
