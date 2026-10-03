#include "aion/gameserver/questEngine/handlers/models/ReportToManyData.h"

#include <memory>
#include <string>

#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/models/NpcInfos.h"
#include "aion/gameserver/questEngine/handlers/template/ReportToMany.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine::handlers::models {

void ReportToManyData::register_(QuestEngine& questEngine) const {
	// the handler keeps pointers to this data's NpcInfos (Java: the same objects, npcInfos.addAll)
	questEngine.addQuestHandler(std::make_unique<template_::ReportToMany>(id, startItemId, startNpcIds, npcInfos, startDialogId, mission));
}

std::optional<std::unordered_set<int32_t>> ReportToManyData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	for (const NpcInfos& npcInfo : npcInfos) {
		if (!npcInfo.getNpcIds()) // Java: NullPointerException on npcIds.size() (npc_ids is required)
			throw runtime::NullPointerException("npc_infos without npc_ids in quest " + std::to_string(id));
		if (auto others = otherNpcIds(*npcInfo.getNpcIds(), npcId))
			return others;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
