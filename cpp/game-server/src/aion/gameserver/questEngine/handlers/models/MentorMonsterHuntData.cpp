#include "aion/gameserver/questEngine/handlers/models/MentorMonsterHuntData.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestKill.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/template/MentorMonsterHunt.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine::handlers::models {

void MentorMonsterHuntData::register_(QuestEngine& questEngine) const {
	// Java: a new list of new Monster objects, or Collections.emptyList(); C++: the handler takes the values
	std::vector<Monster> monsters;
	const gameserver::model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(id);
	if (questTemplate == nullptr) // Java: NullPointerException on getQuestKill()
		throw runtime::NullPointerException("Quest template " + std::to_string(id) + " does not exist");
	if (!questTemplate->getQuestKill().empty()) {
		for (const gameserver::model::templates::quest::QuestKill& qk : questTemplate->getQuestKill()) {
			Monster m;
			if (qk.getKillCount() > 0)
				m.setEndVar(qk.getKillCount());
			// Java: `if (qk.getNpcIds() != null)` - QuestKill.getNpcIds never answers null (an empty list without npc_ids), so always
			m.addNpcIds(qk.getNpcIds());
			if (qk.getVar() > 0)
				m.setVar(qk.getVar());
			if (qk.getQuestStep() > 0)
				m.setStep(qk.getQuestStep());
			if (qk.getSequenceNumber() > 0)
				m.setVar(qk.getSequenceNumber());
			monsters.push_back(std::move(m));
		}
	}
	questEngine.addQuestHandler(std::make_unique<template_::MentorMonsterHunt>(id, startNpcIds, endNpcIds, std::move(monsters), minMenteLevel,
		maxMenteLevel, reward, rewardNextStep));
}

} // namespace aion::gameserver::questEngine::handlers::models
