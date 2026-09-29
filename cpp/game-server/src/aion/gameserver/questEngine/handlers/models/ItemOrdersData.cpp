#include "aion/gameserver/questEngine/handlers/models/ItemOrdersData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/ItemOrders.h"

namespace aion::gameserver::questEngine::handlers::models {

void ItemOrdersData::register_(QuestEngine& questEngine) const {
	questEngine.addQuestHandler(std::make_unique<template_::ItemOrders>(id, talkNpcId1, talkNpcId2, endNpcId));
}

std::optional<std::unordered_set<int32_t>> ItemOrdersData::getAlternativeNpcs(int32_t /*npcId*/) const {
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
