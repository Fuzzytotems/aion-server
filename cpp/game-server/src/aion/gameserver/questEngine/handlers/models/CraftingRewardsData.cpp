#include "aion/gameserver/questEngine/handlers/models/CraftingRewardsData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/CraftingRewards.h"

namespace aion::gameserver::questEngine::handlers::models {

void CraftingRewardsData::register_(QuestEngine& questEngine) const {
	questEngine.addQuestHandler(std::make_unique<template_::CraftingRewards>(id, startNpcId, skillId, levelReward, endNpcId, questMovie));
}

std::optional<std::unordered_set<int32_t>> CraftingRewardsData::getAlternativeNpcs(int32_t /*npcId*/) const {
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
