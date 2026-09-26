#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/ChallengeData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ChallengeData.
 * <p>
 * C++: the index points into the bound `task` storage (Java keeps the list too). getTasks and the searches by quest id follow Java's
 * HashMap<Integer, ChallengeTaskTemplate> iteration order, computed by afterUnmarshal: ChallengeTaskService builds the client's task list in
 * that order, and a quest listed by several tasks finds the same task.
 *
 * @author ViAl
 */
class ChallengeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ChallengeData.xml.inc"
protected:
	std::unordered_map<int32_t, const model::templates::challenge::ChallengeTaskTemplate*> tasksById;

private:
	/** C++ only: tasksById in Java's HashMap iteration order */
	detail::LinkedMap<int32_t, const model::templates::challenge::ChallengeTaskTemplate*> tasksInHashOrder;

public:
	/** Java: the HashMap; C++: a read-only map that iterates in Java's HashMap order */
	const detail::LinkedMap<int32_t, const model::templates::challenge::ChallengeTaskTemplate*>& getTasks() const;

	/** @return the task, nullptr (Java null) if there is none */
	const model::templates::challenge::ChallengeTaskTemplate* getTaskByTaskId(int32_t taskId) const;

	/** @return the first task (Java HashMap order) with the quest, nullptr (Java null) if there is none */
	const model::templates::challenge::ChallengeTaskTemplate* getTaskByQuestId(int32_t questId) const;

	/** @return the quest of the first task (Java HashMap order) with the quest, nullptr (Java null) if there is none */
	const model::templates::challenge::ChallengeQuestTemplate* getQuestByQuestId(int32_t questId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
