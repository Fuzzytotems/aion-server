#include "aion/gameserver/questEngine/handlers/models/ReportOnLevelUpData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/ReportOnLevelUp.h"

namespace aion::gameserver::questEngine::handlers::models {

void ReportOnLevelUpData::register_(QuestEngine& questEngine) const {
	questEngine.addQuestHandler(std::make_unique<template_::ReportOnLevelUp>(id, endNpcIds));
}

std::optional<std::unordered_set<int32_t>> ReportOnLevelUpData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(endNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
