#include "aion/gameserver/questEngine/handlers/models/WorkOrdersData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/WorkOrders.h"

namespace aion::gameserver::questEngine::handlers::models {

void WorkOrdersData::register_(QuestEngine& questEngine) const {
	// the handler keeps pointers to this data's <give_component> elements (Java: the same objects)
	questEngine.addQuestHandler(std::make_unique<template_::WorkOrders>(id, startNpcIds, giveComponents, recipeId));
}

std::optional<std::unordered_set<int32_t>> WorkOrdersData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
