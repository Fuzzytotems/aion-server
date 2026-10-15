#include "aion/gameserver/services/event/EventBuffHandler.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dao/EventDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/InstanceCooltime.h"
#include "aion/gameserver/model/templates/event/BuffMapTypeInfo.h"
#include "aion/gameserver/model/templates/event/BuffRestriction.h"
#include "aion/gameserver/model/templates/event/Buff_Trigger.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/event/Event.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::event {

using model::gameobjects::player::Player;
using model::templates::event::Buff;
using model::templates::event::Buff_TriggerCondition;
using RcIntSet = runtime::RcHashSet<int32_t>;

/** Java ZonedDateTime.getDayOfMonth() */
static int32_t dayOfMonthOf(const utils::time::ServerTime::ZonedDateTime& time) {
	const std::chrono::year_month_day ymd(std::chrono::floor<std::chrono::days>(time.get_local_time()));
	return static_cast<int32_t>(static_cast<unsigned>(ymd.day()));
}

/** Java ZonedDateTime.toLocalDate().lengthOfMonth() */
static int32_t lengthOfMonthOf(const utils::time::ServerTime::ZonedDateTime& time) {
	const std::chrono::year_month_day ymd(std::chrono::floor<std::chrono::days>(time.get_local_time()));
	const std::chrono::year_month_day_last last(ymd.year(), std::chrono::month_day_last(ymd.month()));
	return static_cast<int32_t>(static_cast<unsigned>(last.day()));
}

static runtime::Ref<RcIntSet> toRcSet(const std::vector<int32_t>& values) {
	runtime::Ref<RcIntSet> set = RcIntSet::create(AION_LOCK_CLASS(EventBuffHandler::buffSet));
	for (int32_t value : values)
		set->add(value);
	return set;
}

static runtime::Ref<RcIntSet> toRcSet(const std::unordered_set<int32_t>& values) {
	return toRcSet(std::vector<int32_t>(values.begin(), values.end()));
}

static std::unordered_set<int32_t> toSet(const RcIntSet& values) {
	std::unordered_set<int32_t> set;
	for (int32_t value : values.snapshot())
		set.insert(value);
	return set;
}

/** Java buff.getSkillIds() as a list (Java passes the Set to collectNRandomElements, which copies it into an ArrayList) */
static std::vector<int32_t> skillIdsOf(const Buff& buff) {
	if (!buff.getSkillIds())
		throw runtime::NullPointerException("buff without skill ids"); // Java: new ArrayList<>(null)
	return std::vector<int32_t>(buff.getSkillIds()->begin(), buff.getSkillIds()->end());
}

// Java EventBuffHandler.java:45-50
EventBuffHandler::EventBuffHandler(std::string_view value, const std::vector<const model::templates::event::Buff*>& buffsValue)
	: eventName(std::string(value)), dayOfMonth(dayOfMonthOf(utils::time::ServerTime::now())), effectForceType(Event::getOrCreateEffectForceType(value)) {
	for (const Buff* buff : buffsValue) // Java: this.buffs = buffs
		buffs.add(buff);
	initBuffData();
}

runtime::Ref<EventBuffHandler> EventBuffHandler::create(std::string_view value, const std::vector<const model::templates::event::Buff*>& buffsValue) {
	return runtime::makeRef<EventBuffHandler>(value, buffsValue);
}

// Java EventBuffHandler.java:52-69
void EventBuffHandler::initBuffData() {
	updateActiveBuffSkillIds();
	updateAllowedBuffDays(lengthOfMonthOf(utils::time::ServerTime::now()));
	std::optional<std::vector<dao::EventDAO::StoredBuffData>> buffData = dao::EventDAO::loadStoredBuffData(eventName);
	if (buffData) {
		const std::vector<const Buff*> buffList = buffs.snapshot();
		for (const dao::EventDAO::StoredBuffData& storedBuffData : *buffData) {
			if (storedBuffData.getBuffIndex() < static_cast<int32_t>(buffList.size())) {
				const Buff* buff = buffList[static_cast<size_t>(storedBuffData.getBuffIndex())];
				if (storedBuffData.getActivePoolSkillIds()) {
					if (!buff->getSkillIds())
						throw runtime::NullPointerException("buff without skill ids"); // Java: buff.getSkillIds().containsAll(...)
					bool containsAll = true;
					for (int32_t skillId : *storedBuffData.getActivePoolSkillIds())
						containsAll = containsAll && buff->getSkillIds()->contains(skillId);
					if (containsAll)
						activeBuffPoolSkillIds.put(buff, toRcSet(*storedBuffData.getActivePoolSkillIds()));
				}
				if (storedBuffData.getAllowedBuffDays() && buff->getRestriction() != nullptr
					&& buff->getRestriction()->getRandomDaysPerMonth() == static_cast<int32_t>(storedBuffData.getAllowedBuffDays()->size()))
					allowedBuffDays.put(buff, toRcSet(*storedBuffData.getAllowedBuffDays()));
			}
		}
	}
	storeBuffDataInDb();
}

// Java EventBuffHandler.java:71-76
void EventBuffHandler::updateActiveBuffSkillIds() {
	for (const Buff* buff : buffs.snapshot()) {
		if (buff->getPool() > 0)
			activeBuffPoolSkillIds.put(buff, toRcSet(collectNRandomElements(skillIdsOf(*buff), buff->getPool())));
	}
}

// Java EventBuffHandler.java:78-89
void EventBuffHandler::updateAllowedBuffDays(int32_t endOfMonthDay) {
	for (const Buff* buff : buffs.snapshot()) {
		const int32_t limit = buff->getRestriction() == nullptr ? 0 : buff->getRestriction()->getRandomDaysPerMonth();
		if (limit > 0 && limit < endOfMonthDay) {
			std::vector<int32_t> daysThisMonth;
			for (int32_t day = 1; day <= endOfMonthDay; day++)
				daysThisMonth.push_back(day);
			allowedBuffDays.put(buff, toRcSet(collectNRandomElements(daysThisMonth, limit)));
		} else {
			allowedBuffDays.remove(buff);
		}
	}
}

// Java EventBuffHandler.java:91-101
void EventBuffHandler::storeBuffDataInDb() {
	const std::vector<const Buff*> buffList = buffs.snapshot();
	std::vector<dao::EventDAO::StoredBuffData> storedBuffData;
	for (size_t i = 0; i < buffList.size(); i++) {
		const Buff* buff = buffList[i];
		runtime::Ptr<RcIntSet> poolSkillIds = activeBuffPoolSkillIds.get(buff);
		runtime::Ptr<RcIntSet> allowedDays = allowedBuffDays.get(buff);
		if (poolSkillIds && allowedDays)
			storedBuffData.emplace_back(static_cast<int32_t>(i), toSet(*poolSkillIds), toSet(*allowedDays));
	}
	dao::EventDAO::storeBuffData(eventName, storedBuffData);
}

// Java EventBuffHandler.java:107-117
void EventBuffHandler::onTimeChanged(std::chrono::sys_time<std::chrono::milliseconds> now) {
	const utils::time::ServerTime::ZonedDateTime zonedNow = utils::time::ServerTime::ofEpochMilli(now.time_since_epoch().count());
	const int32_t nowDayOfMonth = dayOfMonthOf(zonedNow);
	if (nowDayOfMonth != dayOfMonth.get()) {
		dayOfMonth.set(nowDayOfMonth);
		updateActiveBuffSkillIds();
		if (dayOfMonth.get() == 1)
			updateAllowedBuffDays(lengthOfMonthOf(zonedNow));
		storeBuffDataInDb();
		resetTodaysBuffs();
	}
}

// Java EventBuffHandler.java:119-121
void EventBuffHandler::resetTodaysBuffs() {
	world::World::getInstance().forEachPlayer([this](Player& player) { onEnterMap(player); });
}

// Java EventBuffHandler.java:123-125
void EventBuffHandler::onEventStop() {
	world::World::getInstance().forEachPlayer([this](Player& player) { endEventBuffs(player); });
}

// Java EventBuffHandler.java:127-130
void EventBuffHandler::onEnterMap(Player& player) {
	endRestrictedEventBuffs(player);
	tryBuff(player, Buff_TriggerCondition::ENTER_MAP);
}

// Java EventBuffHandler.java:132-135
void EventBuffHandler::onEnteredTeam(Player& player, model::team::TemporaryPlayerTeam& team) {
	team.forEach([this](model::gameobjects::AionObject& member) { endRestrictedEventBuffs(*runtime::cast<Player>(member)); });
	tryBuff(player, Buff_TriggerCondition::ENTER_TEAM);
}

// Java EventBuffHandler.java:137-144
void EventBuffHandler::onLeftTeam(Player& player, model::team::TemporaryPlayerTeam& team) {
	endRestrictedEventBuffs(player); // player isn't in team anymore
	team.forEach([this](model::gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		endRestrictedEventBuffs(member);
		// try to apply buffs since some restrictions are now maybe met (team_size_max_percent)
		for (const Buff* buff : buffs.snapshot())
			tryBuff(buff, member, Buff_TriggerCondition::ENTER_TEAM);
	});
}

// Java EventBuffHandler.java:146-149
void EventBuffHandler::onPveKill(Player& killer, model::gameobjects::Npc& victim) {
	if (killer.getLevel() - victim.getLevel() < 10) // victim can be 9 levels below killer level
		tryBuff(killer, Buff_TriggerCondition::PVE_KILL);
}

// Java EventBuffHandler.java:151-153
void EventBuffHandler::onPvpKill(Player& killer, Player& victim) {
	static_cast<void>(victim);
	tryBuff(killer, Buff_TriggerCondition::PVP_KILL);
}

// Java EventBuffHandler.java:155-160
void EventBuffHandler::endEventBuffs(Player& player) {
	for (const runtime::Ptr<skillengine::model::Effect>& effect : player.getEffectController()->getAllEffects()) {
		if (effect->getForceType() == effectForceType)
			effect->endEffect();
	}
}

// Java EventBuffHandler.java:162-172
void EventBuffHandler::endRestrictedEventBuffs(Player& player) {
	for (const runtime::Ptr<skillengine::model::Effect>& effect : player.getEffectController()->getAllEffects()) {
		if (effect->getForceType() == effectForceType) {
			bool stillValid = false;
			for (const Buff* buff : buffs.snapshot()) {
				if (getActiveBuffSkillIds(buff).contains(effect->getSkillId()) && checkRestrictions(buff, player)) {
					stillValid = true; // Java: return - event effect is still valid, check next one
					break;
				}
			}
			if (!stillValid)
				effect->endEffect();
		}
	}
}

// Java EventBuffHandler.java:174-181
bool EventBuffHandler::applyOnTeam(Player& player, const std::function<void(Player&)>& memberAction) {
	runtime::Ptr<model::team::TemporaryPlayerTeam> team = player.getCurrentTeam();
	if (team) {
		team->forEach([&memberAction](model::gameobjects::AionObject& member) { memberAction(*runtime::cast<Player>(member)); });
		return true;
	}
	return false;
}

// Java EventBuffHandler.java:183-188
void EventBuffHandler::tryBuff(Player& player, Buff_TriggerCondition triggerCondition) {
	for (const Buff* buff : buffs.snapshot()) {
		if (!buff->isTeam() || !applyOnTeam(player, [this, buff, triggerCondition](Player& member) { tryBuff(buff, member, triggerCondition); }))
			tryBuff(buff, player, triggerCondition);
	}
}

// Java EventBuffHandler.java:190-204
void EventBuffHandler::tryBuff(const Buff* buff, Player& player, Buff_TriggerCondition triggerCondition) {
	if (canReceiveBuff(buff, player, triggerCondition)) {
		for (int32_t skillId : getActiveBuffSkillIds(buff)) {
			if (player.getEffectController()->hasAbnormalEffect(skillId))
				continue;
			runtime::Ref<skillengine::model::Effect> effect = skillengine::SkillEngine::getInstance().applyEffectDirectly(skillId, player, player,
				buff->isPermanent() ? std::optional<int32_t>(0) : std::nullopt, effectForceType);
			if (effect) {
				const int32_t msgId = 1400697; // You received %0: %1.
				network::aion::serverpackets::SM_SYSTEM_MESSAGE message(model::ChatType::BRIGHT_YELLOW_CENTER,
					runtime::Ptr<model::gameobjects::VisibleObject>(player), msgId,
					std::vector<std::string>{"[Server Buff]", effect->getSkillTemplate()->getL10n()});
				utils::PacketSendUtility::sendPacket(player, message);
			}
		}
	}
}

// Java EventBuffHandler.java:206-208
std::unordered_set<int32_t> EventBuffHandler::getActiveBuffSkillIds(const Buff* buff) {
	runtime::Ptr<RcIntSet> active = activeBuffPoolSkillIds.get(buff);
	if (active)
		return toSet(*active);
	if (!buff->getSkillIds())
		throw runtime::NullPointerException("buff without skill ids"); // Java: getOrDefault returns null, the caller dereferences it
	return *buff->getSkillIds();
}

// Java EventBuffHandler.java:219-226
bool EventBuffHandler::canReceiveBuff(const Buff* buff, Player& player, Buff_TriggerCondition triggerCondition) {
	const Buff::Trigger* trigger = findBuffTrigger(buff, triggerCondition);
	if (trigger == nullptr)
		return false;
	if (!checkRestrictions(buff, player))
		return false;
	return trigger->getChance() == 100 || commons::utils::Rnd::chance() < trigger->getChance();
}

// Java EventBuffHandler.java:228-234
const Buff::Trigger* EventBuffHandler::findBuffTrigger(const Buff* buff, Buff_TriggerCondition triggerCondition) {
	for (const Buff::Trigger& trigger : buff->getTriggers()) {
		if (trigger.getCondition() == triggerCondition)
			return &trigger;
	}
	return nullptr;
}

// Java EventBuffHandler.java:236-244
bool EventBuffHandler::checkRestrictions(const Buff* buff, Player& player) {
	if (!isAllowedToday(buff, player))
		return false;
	if (!isAllowedOnCurrentMap(buff, player))
		return false;
	if (!isAllowedTeamSize(buff, player))
		return false;
	return true;
}

// Java EventBuffHandler.java:246-259
bool EventBuffHandler::isAllowedTeamSize(const Buff* buff, Player& player) {
	if (buff->getRestriction() == nullptr || buff->getRestriction()->getTeamSizeMaxPercent() == 0)
		return true;
	runtime::Ptr<model::team::TemporaryPlayerTeam> team = player.getCurrentTeam();
	if (team) {
		int32_t maxAllowedTeamSize = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getMaxMemberCount(player.getWorldId(), player.getRace());
		if (maxAllowedTeamSize == 0)
			maxAllowedTeamSize = team->getMaxMemberCount();
		if (buff->getRestriction()->getTeamSizeMaxPercent() >= static_cast<float>(team->size()) * 100.0f / static_cast<float>(maxAllowedTeamSize))
			return true;
	}
	return false;
}

// Java EventBuffHandler.java:261-264
bool EventBuffHandler::isAllowedToday(const Buff* buff, Player& player) {
	static_cast<void>(player);
	runtime::Ptr<RcIntSet> allowedDays = this->allowedBuffDays.get(buff);
	return !allowedDays || allowedDays->contains(dayOfMonth.get());
}

// Java EventBuffHandler.java:266-279
bool EventBuffHandler::isAllowedOnCurrentMap(const Buff* buff, Player& player) {
	if (buff->getRestriction() == nullptr || !buff->getRestriction()->getMaps())
		return true;
	runtime::Ptr<world::WorldMapInstance> worldMapInstance = player.getPosition()->getWorldMapInstance();
	for (model::templates::event::Buff_BuffMapType buffMapType : *buff->getRestriction()->getMaps()) {
		if (model::templates::event::matches(buffMapType, *worldMapInstance)) {
			if (buffMapType == model::templates::event::Buff_BuffMapType::WORLD_MAP)
				return true;
			if (checkInstanceLevel(player))
				return true;
		}
	}
	return false;
}

// Java EventBuffHandler.java:281-289
bool EventBuffHandler::checkInstanceLevel(Player& player) {
	// only allow if the player level is not too high (max 9 levels above the instance entry level)
	const model::templates::InstanceCooltime* cooltime = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getInstanceCooltimeByWorldId(player.getWorldId());
	if (cooltime != nullptr) {
		const int32_t instanceLevel = player.getRace() == model::Race::ELYOS ? cooltime->getEnterMinLevelLight() : cooltime->getEnterMinLevelDark();
		return instanceLevel == 0 || player.getLevel() - instanceLevel < 10;
	}
	return true;
}

EventBuffHandler::~EventBuffHandler() = default;

} // namespace aion::gameserver::services::event
