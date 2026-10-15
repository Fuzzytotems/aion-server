#include "aion/gameserver/questEngine/handlers/models/KillInZoneData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/KillInZone.h"

namespace aion::gameserver::questEngine::handlers::models {

void KillInZoneData::register_(QuestEngine& questEngine) const {
	questEngine.addQuestHandler(
		std::make_unique<template_::KillInZone>(id, endNpcIds, startNpcIds, zones, amount, minRank, levelDiff, startDistanceNpc));
}

std::optional<std::unordered_set<int32_t>> KillInZoneData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	if (auto others = otherNpcIds(endNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
