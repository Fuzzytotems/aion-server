#pragma once

#include <cstdint>
#include <functional>
#include <unordered_set>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/quest/fwd.h"

namespace aion::gameserver::model::templates::quest {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Not static data despite its package: the quest registrations of an NPC id
 * (fieldmap K4, `QuestEngine.questNpcs`), RefCounted, created with create(); the constructors only create the collections (ported). The Java
 * getters return the live collections: references to the shims (§7.1).
 *
 * @author MrPoke
 */
class QuestNpc : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::HashSet<int32_t> onQuestStart{AION_LOCK_CLASS(QuestNpc::onQuestStart)};
	runtime::ArrayList<int32_t> onKillEvent{AION_LOCK_CLASS(QuestNpc::onKillEvent)};
	runtime::ArrayList<int32_t> onTalkEvent{AION_LOCK_CLASS(QuestNpc::onTalkEvent)};
	runtime::ArrayList<int32_t> onAttackEvent{AION_LOCK_CLASS(QuestNpc::onAttackEvent)};
	runtime::ArrayList<int32_t> onAddAggroListEvent{AION_LOCK_CLASS(QuestNpc::onAddAggroListEvent)};
	runtime::ArrayList<int32_t> onAtDistanceEvent{AION_LOCK_CLASS(QuestNpc::onAtDistanceEvent)};
	const int32_t npcId;
	const int32_t questRange;

protected:
	QuestNpc(int32_t npcId, int32_t questRange);
	explicit QuestNpc(int32_t npcId);
	~QuestNpc() override;

public:
	/** Java: new QuestNpc(npcId, questRange) */
	static runtime::Ref<QuestNpc> create(int32_t npcId, int32_t questRange);

	/** Java: new QuestNpc(npcId) (quest range 20) */
	static runtime::Ref<QuestNpc> create(int32_t npcId);

	void addOnQuestStart(int32_t questId);

	runtime::HashSet<int32_t>& getOnQuestStart() { return onQuestStart; }

	void addOnAttackEvent(int32_t questId);

	runtime::ArrayList<int32_t>& getOnAttackEvent() { return onAttackEvent; }

	void addOnKillEvent(int32_t questId);

	runtime::ArrayList<int32_t>& getOnKillEvent() { return onKillEvent; }

	void addOnTalkEvent(int32_t questId);

	runtime::ArrayList<int32_t>& getOnTalkEvent() { return onTalkEvent; }

	void addOnAddAggroListEvent(int32_t questId);

	runtime::ArrayList<int32_t>& getOnAddAggroListEvent() { return onAddAggroListEvent; }

	void addOnAtDistanceEvent(int32_t questId);

	runtime::ArrayList<int32_t>& getOnDistanceEvent() { return onAtDistanceEvent; }

	int32_t getNpcId() const { return npcId; }

	int32_t getQuestRange() const { return questRange; }

	std::unordered_set<int32_t> findAllRegisteredQuestIds(const std::function<bool(int32_t)>& questIdFilter);
};

} // namespace aion::gameserver::model::templates::quest
