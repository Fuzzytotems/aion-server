#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/fwd.h"

namespace aion::gameserver::model::gameobjects::player::npcFaction {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `NpcFactions.factions`), created with create(). The
 * constructor reads the faction template (DataManager), so it stays `AION_UNPORTED` after the member initializers.
 *
 * @author MrPoke
 */
class NpcFaction : public runtime::RefCounted, public Persistable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t id;
	runtime::Field<int32_t> time{};
	runtime::Field<bool> active{};
	const bool mentor; // Java: DataManager.NPC_FACTIONS_DATA.getNpcFactionById(id).isMentor() (constructor)
	runtime::Field<ENpcFactionQuestState> state{};
	runtime::Field<int32_t> questId{};
	runtime::Field<PersistentState> persistentState; // Java: = PersistentState.NEW (constructor)

protected:
	NpcFaction(int32_t id, int32_t time, bool active, ENpcFactionQuestState state, int32_t questId);
	~NpcFaction() override;

public:
	/** Java: new NpcFaction(id, time, active, state, questId) */
	static runtime::Ref<NpcFaction> create(int32_t id, int32_t time, bool active, ENpcFactionQuestState state, int32_t questId);

	int32_t getId() const { return id; }

	int32_t getTime() const { return time.get(); }

	bool isActive() const { return active.get(); }

	bool isMentor() const { return mentor; }

	ENpcFactionQuestState getState() const { return state.get(); }

	void setTime(int32_t time);

	void setActive(bool active);

	void setState(ENpcFactionQuestState state);

	int32_t getQuestId() const { return questId.get(); }

	void setQuestId(int32_t questId);

	PersistentState getPersistentState() override { return persistentState.get(); }

	void setPersistentState(PersistentState persistentState) override;
};

} // namespace aion::gameserver::model::gameobjects::player::npcFaction
