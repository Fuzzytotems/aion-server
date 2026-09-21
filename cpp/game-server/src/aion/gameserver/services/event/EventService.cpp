#include "aion/gameserver/services/event/EventService.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/EventData.h"
#include "aion/gameserver/model/EventTheme.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_VERSION_CHECK.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/event/Event.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services::event {

using EventSet = runtime::RcHashSet<runtime::Ref<Event>>;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.event.EventService");

/** Java Collections.emptySet() for activeEvents: the port creates the empty set where Java reads it (hub-headers.md §11.1) */
static runtime::Ref<EventSet> newEventSet() {
	return EventSet::create(AION_LOCK_CLASS(EventService::activeEvents));
}

/** the current active events; the field is null until the first check (Java: Collections.emptySet()) */
static std::vector<runtime::Ptr<Event>> snapshotOf(runtime::Ptr<EventSet> events) {
	std::vector<runtime::Ptr<Event>> snapshot;
	if (events) {
		for (const runtime::Ptr<Event>&event : events->snapshot())
			snapshot.emplace_back(event);
	}
	return snapshot;
}

// Java field initializers: Collections.emptySet()/emptyList(); the singleton is created on first use (not during static initialization), so the
// empty collections exist from the start and every reader sees a collection, as in Java
EventService::EventService()
	: activeEvents(newEventSet()),
	  activeEventDropRules(runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>::create(AION_LOCK_CLASS(EventService::activeEventDropRules))),
	  activeEventQuests(runtime::RcHashSet<int32_t>::create(AION_LOCK_CLASS(EventService::activeEventQuests))),
	  effectForceTypes(runtime::RcHashSet<const skillengine::model::Effect_ForceType*>::create(AION_LOCK_CLASS(EventService::effectForceTypes))) {
}

EventService::~EventService() = default;

EventService& EventService::getInstance() {
	static EventService instance; // Java SingletonHolder
	return instance;
}

// callback at EventService.java:51 (fieldmap key EventService@L51:50)
bool EventService::start() {
	if (checkTask.get())
		return false;

	validateConfiguredEventNames();
	checkActiveEvents();
	checkTask.set(cron::CronService::getInstance().schedule(runtime::PinnedCallback<void()>(runtime::Pin{this}, [this] { onTimeChanged(); }),
		"0 0/5 * ? * *"));
	return true;
}

void EventService::stop() {
	if (runtime::Ptr<cron::JobDetail> task = checkTask.get()) {
		cron::CronService::getInstance().cancel(runtime::Ref<cron::JobDetail>(task));
		checkTask.set(nullptr);
		runtime::Ptr<EventSet> oldActiveEvents = activeEvents.get();
		std::vector<runtime::Ptr<Event>> oldEvents = snapshotOf(oldActiveEvents);
		runtime::Ref<EventSet> retainedOldEvents = oldActiveEvents ? runtime::Ref<EventSet>(oldActiveEvents) : nullptr;
		activeEvents.set(newEventSet());
		activeEventQuests.set(runtime::RcHashSet<int32_t>::create(AION_LOCK_CLASS(EventService::activeEventQuests)));
		activeEventDropRules.set(
			runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>::create(AION_LOCK_CLASS(EventService::activeEventDropRules)));
		updateEventTheme();
		for (const runtime::Ptr<Event>& event : oldEvents) // iterate after emptying activeEvents to ensure correct handling in stop()
			event->stop();
	}
}

void EventService::validateConfiguredEventNames() {
	std::shared_ptr<const std::unordered_set<std::string>> disabledEvents = configs::main::EventsConfig::DISABLED_EVENTS.get();
	const std::unordered_set<std::string> noEvents;
	const std::unordered_set<std::string>& disabled = disabledEvents ? *disabledEvents : noEvents;
	if (!isAllEvents(disabled)) {
		std::unordered_set<std::string> eventNames;
		for (const model::templates::event::EventTemplate& eventTemplate : dataholders::DataManager::EVENT_DATA->getEvents())
			eventNames.insert(eventTemplate.getName());
		for (const std::string& eventName : disabled) {
			if (!eventNames.contains(eventName))
				log.warn("Unknown event \"" + eventName + "\" configured as disabled");
		}
	}
}

void EventService::checkActiveEvents() {
	runtime::Ptr<EventSet> oldActiveEvents = activeEvents.get();
	runtime::Ref<EventSet> oldEvents = oldActiveEvents ? runtime::Ref<EventSet>(oldActiveEvents) : newEventSet();
	runtime::Ref<EventSet> newActiveEvents = collectActiveEvents();
	// Java: !oldActiveEvents.equals(newActiveEvents) (set equality of the identical Event objects)
	std::vector<runtime::Ptr<Event>> oldSnapshot = oldEvents->snapshot();
	bool equal = static_cast<int32_t>(oldSnapshot.size()) == newActiveEvents->size()
		&& std::ranges::all_of(oldSnapshot, [&newActiveEvents](const runtime::Ptr<Event>&event) { return newActiveEvents->contains(event); });
	if (!equal) {
		activeEvents.set(newActiveEvents);
		activeEventQuests.set(collectQuestIds(*newActiveEvents));
		activeEventDropRules.set(collectDropRules(*newActiveEvents));
		updateEventTheme();
		startOrStopEvents(*oldEvents, *newActiveEvents);
		runtime::Ref<runtime::RcHashSet<const skillengine::model::Effect_ForceType*>> forceTypes =
			runtime::RcHashSet<const skillengine::model::Effect_ForceType*>::create(AION_LOCK_CLASS(EventService::effectForceTypes));
		for (const runtime::Ptr<Event>& event : snapshotOf(newActiveEvents)) {
			if (const skillengine::model::Effect_ForceType* forceType = event->getEffectForceType())
				forceTypes->add(forceType);
		}
		effectForceTypes.set(std::move(forceTypes));
	}
}

bool EventService::isInactiveEventForceType(const skillengine::model::Effect_ForceType* forceType) {
	runtime::Ptr<runtime::RcHashSet<const skillengine::model::Effect_ForceType*>> forceTypes = effectForceTypes.get();
	return Event::isEventEffectForceType(forceType) && !(forceTypes && forceTypes->contains(forceType));
}

void EventService::onTimeChanged() {
	checkActiveEvents();
	std::vector<runtime::Ptr<Event>> events = snapshotOf(activeEvents.get());
	if (!events.empty()) {
		utils::time::ServerTime::ZonedDateTime now = utils::time::ServerTime::now();
		for (const runtime::Ptr<Event>& event : events)
			event->onTimeChanged(now.get_sys_time());
	}
}

void EventService::onPlayerLogin(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get()))
		event->onPlayerLogin(player);
}

void EventService::onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get()))
		event->onEnteredTeam(player, team);
}

void EventService::onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get()))
		event->onLeftTeam(player, team);
}

void EventService::onEnterMap(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get()))
		event->onEnterMap(player);
}

void EventService::onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim) {
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get()))
		event->onPveKill(killer, victim);
}

void EventService::onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim) {
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get()))
		event->onPvpKill(killer, victim);
}

bool EventService::isAllEvents(const std::unordered_set<std::string>& list) {
	return list.size() == 1 && *list.begin() == "*";
}

runtime::Ref<EventSet> EventService::collectActiveEvents() {
	std::shared_ptr<const std::unordered_set<std::string>> disabledEvents = configs::main::EventsConfig::DISABLED_EVENTS.get();
	const std::unordered_set<std::string> noEvents;
	const std::unordered_set<std::string>& disabled = disabledEvents ? *disabledEvents : noEvents;
	runtime::Ref<EventSet> events = newEventSet();
	if (isAllEvents(disabled))
		return events; // Java: Collections.emptySet()
	utils::time::ServerTime::LocalDateTime now = utils::time::ServerTime::now().get_local_time();
	for (const model::templates::event::EventTemplate& eventTemplate : dataholders::DataManager::EVENT_DATA->getEvents()) {
		if (!disabled.contains(eventTemplate.getName()) && eventTemplate.isInEventPeriod(now))
			events->add(findOrCreateEvent(&eventTemplate));
	}
	return events;
}

runtime::Ref<Event> EventService::findOrCreateEvent(const model::templates::event::EventTemplate* et) {
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get())) {
		if (et == event->getEventTemplate()) // Java: et.equals(event.getEventTemplate()), identity (EventTemplate does not override equals)
			return runtime::Ref<Event>(event);
	}
	return Event::create(et);
}

void EventService::startOrStopEvents(EventSet& oldActiveEvents, EventSet& newActiveEvents) {
	for (const runtime::Ptr<Event>&oldActiveEvent : oldActiveEvents.snapshot()) {
		if (!newActiveEvents.contains(oldActiveEvent))
			oldActiveEvent->stop();
	}
	if (!newActiveEvents.isEmpty()) {
		bool cleanedOldBuffData = false;
		for (const runtime::Ptr<Event>&newActiveEvent : newActiveEvents.snapshot()) {
			if (!oldActiveEvents.contains(newActiveEvent)) {
				if (!cleanedOldBuffData) {
					cleanedOldBuffData = true;
					// Java: EventDAO.deleteOldBuffData(); the event buff data belongs to the events work (plan O-11)
					AION_UNPORTED();
				}
				newActiveEvent->start();
			}
		}
	}
}

runtime::Ref<runtime::RcHashSet<int32_t>> EventService::collectQuestIds(EventSet& events) {
	runtime::Ref<runtime::RcHashSet<int32_t>> questIds = runtime::RcHashSet<int32_t>::create(AION_LOCK_CLASS(EventService::activeEventQuests));
	for (const runtime::Ptr<Event>&event : events.snapshot()) {
		questIds->addAll(event->getEventTemplate()->getStartableQuests());
		questIds->addAll(event->getEventTemplate()->getMaintainableQuests());
	}
	return questIds;
}

runtime::Ref<runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>> EventService::collectDropRules(EventSet& events) {
	runtime::Ref<runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>> dropRules =
		runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>::create(AION_LOCK_CLASS(EventService::activeEventDropRules));
	for (const runtime::Ptr<Event>&event : events.snapshot()) {
		const auto& eventDropRules = event->getEventTemplate()->getEventDropRules();
		if (eventDropRules) {
			for (const model::templates::globaldrops::GlobalRule& rule : *eventDropRules)
				dropRules->add(&rule);
		}
	}
	return dropRules;
}

bool EventService::isEventActive(std::string_view eventName) {
	return std::ranges::any_of(snapshotOf(activeEvents.get()),
		[eventName](const runtime::Ptr<Event>& e) { return e->getEventTemplate()->getName() == eventName; });
}

bool EventService::isActiveEventQuest(int32_t questId) {
	runtime::Ptr<runtime::RcHashSet<int32_t>> quests = activeEventQuests.get();
	return quests && quests->contains(questId);
}

commons::configuration::Properties EventService::getActiveEventConfigProperties() {
	commons::configuration::Properties eventConfigProperties;
	std::vector<const model::templates::event::EventTemplate*> templates;
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get())) {
		if (event->getEventTemplate()->hasConfigProperties())
			templates.push_back(event->getEventTemplate());
	}
	// Java: Comparator.nullsFirst(Comparator.comparing(EventTemplate::getStartDate)). nullsFirst null-checks the compared *elements*, not the
	// extracted key, so Comparator.comparing dereferences a null start date and Java throws NullPointerException here (an event without a start
	// date is active, EventTemplate.isInEventPeriod). The port orders those templates first instead, which is what the Java author meant
	// (deviation, docs/deviations/P5-12b.md). Stable, like the sort of a Java stream.
	std::ranges::stable_sort(templates, [](const model::templates::event::EventTemplate* a, const model::templates::event::EventTemplate* b) {
		const auto& startA = a->getStartDate();
		const auto& startB = b->getStartDate();
		if (!startA || !startB)
			return !startA && startB.has_value();
		return *startA < *startB;
	});
	for (const model::templates::event::EventTemplate* et : templates) {
		try {
			eventConfigProperties.putAll(et->loadConfigProperties());
		} catch (const std::exception& e) {
			log.error("Could not load config properties of event " + et->getName(), e);
		}
	}
	return eventConfigProperties;
}

void EventService::updateEventTheme() {
	model::EventTheme oldEventTheme = eventTheme.get();
	model::EventTheme newEventTheme = model::EventTheme::NONE;
	for (const runtime::Ptr<Event>& event : snapshotOf(activeEvents.get())) {
		if (event->getEventTemplate()->getTheme()) {
			newEventTheme = *event->getEventTemplate()->getTheme();
			break;
		}
	}
	if (oldEventTheme != newEventTheme) {
		eventTheme.set(newEventTheme);
		// update city decoration (logged in players see changes after teleport)
		utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_VERSION_CHECK(newEventTheme));
	}
}

} // namespace aion::gameserver::services::event
