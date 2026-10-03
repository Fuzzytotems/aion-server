#include "aion/gameserver/questEngine/handlers/models/SkillUseData.h"

#include <memory>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/template/SkillUse.h"

namespace aion::gameserver::questEngine::handlers::models {

void SkillUseData::register_(QuestEngine& questEngine) const {
	// the handler keeps pointers to this data's <skill> elements (Java: the same objects)
	questEngine.addQuestHandler(std::make_unique<template_::SkillUse>(id, startNpcIds, endNpcIds, skills));
}

std::optional<std::unordered_set<int32_t>> SkillUseData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	if (auto others = otherNpcIds(endNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
