#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/services/event/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::skillengine::model {
class Effect_ForceType;
} // namespace aion::gameserver::skillengine::model

namespace aion::gameserver::services::event {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze. A per-run
 * service object (RefCounted, conventions-game-server.md). Effect.ForceType is the interned `const Effect_ForceType*` (Effect.h).
 *
 * @author Neon
 */
class Event : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	static constexpr std::string_view EFFECT_FORCE_TYPE_PREFIX = "[EVENT] ";
	const model::templates::event::EventTemplate* eventTemplate;
	runtime::AtomicBoolean started{AION_LOCK_CLASS(Event::started)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::Ref<EventBuffHandler>> eventBuffHandler{};
	runtime::Field<runtime::FutureRef> inventoryDropTask{};

	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::PinnedCallback<void()>>>> onEventEndTasks{};

protected:
	explicit Event(const model::templates::event::EventTemplate* eventTemplate);

public:
	static runtime::Ref<Event> create(const model::templates::event::EventTemplate* value);

	const model::templates::event::EventTemplate* getEventTemplate() const { return this->eventTemplate; }

	static const skillengine::model::Effect_ForceType* getOrCreateEffectForceType(std::string_view identifier);

	static bool isEventEffectForceType(const skillengine::model::Effect_ForceType* forceType);

	void start();

	void stop();

	const skillengine::model::Effect_ForceType* getEffectForceType();

	/** @param now Java ZonedDateTime in the server time zone (hub-headers.md §6) */
	void onTimeChanged(std::chrono::sys_time<std::chrono::milliseconds> now);

	void onPlayerLogin(model::gameobjects::player::Player& player);

	void onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team);

	void onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team);

	void onEnterMap(model::gameobjects::player::Player& player);

	void onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim);

	void onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim);

private:
	void startOrMaintainQuests(model::gameobjects::player::Player& player);

	bool isAllowedToStartEventQuest(model::gameobjects::player::Player& player, int32_t questId);

	void despawnNonEventSpawns(int32_t npcId, world::WorldMap& worldMap);

public:
	/** @param task stored until the event ends (hub-headers.md §7.3) */
	bool addOnEventEndTask(runtime::PinnedCallback<void()> task);

protected:
	~Event() override;
};

} // namespace aion::gameserver::services::event
