#include "aion/gameserver/questEngine/handlers/models/RelicRewardsData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/RelicRewards.h"

namespace aion::gameserver::questEngine::handlers::models {

void RelicRewardsData::register_(QuestEngine& questEngine) const {
	questEngine.addQuestHandler(std::make_unique<template_::RelicRewards>(id, startNpcIds));
}

std::optional<std::unordered_set<int32_t>> RelicRewardsData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
