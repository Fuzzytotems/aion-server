#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/configuration/Properties.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/EventTheme.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/model/templates/globaldrops/fwd.h"
#include "aion/gameserver/services/event/fwd.h"

namespace aion::gameserver::skillengine::model {
class Effect_ForceType;
} // namespace aion::gameserver::skillengine::model

namespace aion::gameserver::services::cron {
class JobDetail;
} // namespace aion::gameserver::services::cron

namespace aion::gameserver::services::event {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * TemporaryPlayerTeam<? extends TeamMember<Player>> is the erased TemporaryPlayerTeam (§8.1); the volatile collection fields are
 * Field<Ref<RcX>> and their getters return the current collection (§7.1); Properties is the commons class.
 *
 * @author Rolandas, Neon
 */
class EventService : public runtime::Immortal {
private:
	runtime::Field<runtime::Ref<cron::JobDetail>> checkTask{}; // fieldmap.toml: Quartz JobDetail is services::cron::JobDetail (hub-headers.md §6)
	runtime::Field<runtime::Ref<runtime::RcHashSet<runtime::Ref<Event>>>> activeEvents{}; // Java: = Collections.emptySet(); null until the first check (the port creates the empty set)
	runtime::Field<runtime::Ref<runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>>> activeEventDropRules{}; // Java: = Collections.emptyList()
	runtime::Field<runtime::Ref<runtime::RcHashSet<int32_t>>> activeEventQuests{}; // Java: = Collections.emptySet()
	// fieldmap.toml: Effect.ForceType is the interned Effect_ForceType (Effect.h: using ForceType = Effect_ForceType); Java: = Collections.emptySet()
	runtime::Field<runtime::Ref<runtime::RcHashSet<const skillengine::model::Effect_ForceType*>>> effectForceTypes{};
	runtime::Field<model::EventTheme> eventTheme{model::EventTheme::NONE};
	EventService();
	~EventService();
public:
	bool start();
	void stop();
private:
	void validateConfiguredEventNames();
	void checkActiveEvents();
public:
	bool isInactiveEventForceType(const skillengine::model::Effect_ForceType* forceType);
private:
	void onTimeChanged();
public:
	void onPlayerLogin(model::gameobjects::player::Player& player);
	void onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team);
	void onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team);
	void onEnterMap(model::gameobjects::player::Player& player);
	void onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim);
	void onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim);
private:
	bool isAllEvents(const std::unordered_set<std::string>& list);
	/** @return the new set that checkActiveEvents stores into activeEvents (hub-headers.md §7.1: the collections built here become fields) */
	runtime::Ref<runtime::RcHashSet<runtime::Ref<Event>>> collectActiveEvents();
	runtime::Ref<Event> findOrCreateEvent(const model::templates::event::EventTemplate* et);
	void startOrStopEvents(runtime::RcHashSet<runtime::Ref<Event>>& oldActiveEvents, runtime::RcHashSet<runtime::Ref<Event>>& newActiveEvents);
	runtime::Ref<runtime::RcHashSet<int32_t>> collectQuestIds(runtime::RcHashSet<runtime::Ref<Event>>& events);
	runtime::Ref<runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>> collectDropRules(runtime::RcHashSet<runtime::Ref<Event>>& events);
public:
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<Event>>> getActiveEvents() const { return this->activeEvents.get(); }
	bool isEventActive(std::string_view eventName);
	bool isActiveEventQuest(int32_t questId);
	commons::configuration::Properties getActiveEventConfigProperties();
private:
	void updateEventTheme();
public:
	model::EventTheme getEventTheme() const { return this->eventTheme.get(); }
	runtime::Ptr<runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>> getActiveEventDropRules() const {
		return this->activeEventDropRules.get();
	}
	static EventService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services::event
