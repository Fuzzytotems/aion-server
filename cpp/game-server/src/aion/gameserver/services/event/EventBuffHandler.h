#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/templates/event/Buff.h"
#include "aion/gameserver/model/templates/event/Buff_TriggerCondition.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/services/event/fwd.h"

namespace aion::gameserver::skillengine::model {
class Effect_ForceType;
} // namespace aion::gameserver::skillengine::model

namespace aion::gameserver::services::event {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Neon
 */
class EventBuffHandler : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const std::string eventName;
	runtime::ArrayList<const model::templates::event::Buff*> buffs{AION_LOCK_CLASS(EventBuffHandler::buffs)};
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<const model::templates::event::Buff*, runtime::Ref<runtime::RcHashSet<int32_t>>> activeBuffPoolSkillIds{
		AION_LOCK_CLASS(EventBuffHandler::activeBuffPoolSkillIds#stripe)};
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<const model::templates::event::Buff*, runtime::Ref<runtime::RcHashSet<int32_t>>> allowedBuffDays{
		AION_LOCK_CLASS(EventBuffHandler::allowedBuffDays#stripe)};
	runtime::Field<int32_t> dayOfMonth{}; // Java: = ServerTime.now().getDayOfMonth()
	// fieldmap: Effect.ForceType is the interned Effect_ForceType (Effect.h: using ForceType = Effect_ForceType)
	const skillengine::model::Effect_ForceType* effectForceType;

protected:
	EventBuffHandler(std::string_view eventName, const std::vector<const model::templates::event::Buff*>& buffs);

public:
	static runtime::Ref<EventBuffHandler> create(std::string_view value, const std::vector<const model::templates::event::Buff*>& buffsValue);

private:
	void initBuffData();

	void updateActiveBuffSkillIds();

	void updateAllowedBuffDays(int32_t endOfMonthDay);

	void storeBuffDataInDb();

public:
	const skillengine::model::Effect_ForceType* getEffectForceType() const { return this->effectForceType; }

	/** @param now Java ZonedDateTime in the server time zone (hub-headers.md §6) */
	void onTimeChanged(std::chrono::sys_time<std::chrono::milliseconds> now);

private:
	void resetTodaysBuffs();

public:
	void onEventStop();

	void onEnterMap(model::gameobjects::player::Player& player);

	void onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team);

	void onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team);

	void onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim);

	void onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim);

private:
	void endEventBuffs(model::gameobjects::player::Player& player);

	void endRestrictedEventBuffs(model::gameobjects::player::Player& player);

	bool applyOnTeam(model::gameobjects::player::Player& player, const std::function<void(model::gameobjects::player::Player&)>& memberAction);

	void tryBuff(model::gameobjects::player::Player& player, model::templates::event::Buff_TriggerCondition triggerCondition);

	void tryBuff(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player,
		model::templates::event::Buff_TriggerCondition triggerCondition);

	std::unordered_set<int32_t> getActiveBuffSkillIds(const model::templates::event::Buff* buff);

	/** Java generic method: Set<T> of n random elements of the collection (hub-headers.md §8.3) */
	template <class T>
	std::vector<T> collectNRandomElements(const std::vector<T>& input, int32_t n) { AION_UNPORTED(); }

	bool canReceiveBuff(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player,
		model::templates::event::Buff_TriggerCondition triggerCondition);

	const model::templates::event::Buff::Trigger* findBuffTrigger(const model::templates::event::Buff* buff,
		model::templates::event::Buff_TriggerCondition triggerCondition);

	bool checkRestrictions(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player);

	bool isAllowedTeamSize(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player);

	bool isAllowedToday(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player);

	bool isAllowedOnCurrentMap(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player);

	bool checkInstanceLevel(model::gameobjects::player::Player& player);

protected:
	~EventBuffHandler() override;
};

} // namespace aion::gameserver::services::event
