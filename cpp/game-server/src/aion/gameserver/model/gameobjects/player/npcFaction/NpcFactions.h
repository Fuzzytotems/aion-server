#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"

namespace aion::gameserver::model::gameobjects::player::npcFaction {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`PartSlot<NpcFactions>`, cycles review), bound to the player in
 * the constructor. The anonymous RequestResponseHandler of askLeaveNpcFaction is the fieldmap callback struct NpcFactions_RequestResponseHandler,
 * defined in NpcFactions.cpp (§7.3).
 *
 * @author MrPoke, synchro2, Sykra
 */
class NpcFactions : public runtime::OwnedPart {
	/** C++ only: the callback struct of askLeaveNpcFaction (Java anonymous inner class) calls the private leaveNpcFaction(NpcFaction&) */
	friend class NpcFactions_RequestResponseHandler;

private:
	runtime::OwnerRef<Player> owner;
	runtime::HashMap<int32_t, runtime::Ref<NpcFaction>> factions{AION_LOCK_CLASS(NpcFactions::factions)};
	const runtime::Ref<runtime::Array<runtime::Ref<NpcFaction>>> activeNpcFaction; // Java: = new NpcFaction[2] (constructor)
	const runtime::Ref<runtime::Array<int32_t>> timeLimit; // Java: = new int[] { 0, 0 } (constructor)

public:
	explicit NpcFactions(Player& owner);

	~NpcFactions() override;

	void addNpcFaction(NpcFaction& faction);

	runtime::Ptr<NpcFaction> getFactionById(int32_t id);

	std::vector<runtime::Ptr<NpcFaction>> getNpcFactions();

	runtime::Ptr<NpcFaction> getActiveNpcFaction(bool mentor);

	runtime::Ptr<NpcFaction> setActive(int32_t npcFactionId);

	void leaveNpcFaction(Npc& npc);

private:
	void leaveNpcFaction(NpcFaction& npcFaction);

public:
	void enterGuild(Npc& npc);

private:
	void askLeaveNpcFaction(Npc& npc);

public:
	void startQuest(const templates::QuestTemplate* questTemplate);

	void abortQuest(const templates::QuestTemplate* questTemplate);

	void completeQuest(const templates::QuestTemplate* questTemplate);

	void sendDailyQuest();

	void onLevelUp();

private:
	int32_t getNextTime();

public:
	bool canStartQuest(const templates::QuestTemplate* template_);
};

} // namespace aion::gameserver::model::gameobjects::player::npcFaction
