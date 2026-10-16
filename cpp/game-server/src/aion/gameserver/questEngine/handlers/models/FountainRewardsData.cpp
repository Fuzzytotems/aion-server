#include "aion/gameserver/questEngine/handlers/models/FountainRewardsData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/FountainRewards.h"

namespace aion::gameserver::questEngine::handlers::models {

void FountainRewardsData::register_(QuestEngine& questEngine) const {
	questEngine.addQuestHandler(std::make_unique<template_::FountainRewards>(id, startNpcIds));
}

std::optional<std::unordered_set<int32_t>> FountainRewardsData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
