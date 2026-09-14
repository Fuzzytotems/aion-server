#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.questStateList`), created with create().
 *
 * @author MrPoke, vlog, Neon
 */
class QuestStateList : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	// Java: private static final Logger log = LoggerFactory.getLogger(QuestStateList.class) - namespace-scope logger in QuestStateList.cpp
	runtime::TreeMap<int32_t, runtime::Ref<questEngine::model::QuestState>> quests{AION_LOCK_CLASS(QuestStateList::quests)};
	runtime::HashSet<int32_t> deletedQuests{AION_LOCK_CLASS(QuestStateList::deletedQuests)};

protected:
	/** Creates a new instance of QuestStateList */
	QuestStateList();
	~QuestStateList() override;

public:
	/** Java: new QuestStateList() */
	static runtime::Ref<QuestStateList> create();

	bool hasQuest(int32_t questId);

	/** synchronized */
	bool addQuest(int32_t questId, questEngine::model::QuestState& questState);

	/** synchronized. @return the removed quest state, null if there was none */
	runtime::Ptr<questEngine::model::QuestState> deleteQuest(int32_t questId);

	runtime::Ptr<questEngine::model::QuestState> getQuestState(int32_t questId);

	std::vector<runtime::Ptr<questEngine::model::QuestState>> getAllQuestState();

	std::vector<runtime::Ptr<questEngine::model::QuestState>> getCompletedQuests();

	std::vector<runtime::Ptr<questEngine::model::QuestState>> getUncompletedQuests();

	std::vector<runtime::Ptr<questEngine::model::QuestState>> getNormalQuests();

	runtime::HashSet<int32_t>& getDeletedQuestIds() { return deletedQuests; }
};

} // namespace aion::gameserver::model::gameobjects::player
