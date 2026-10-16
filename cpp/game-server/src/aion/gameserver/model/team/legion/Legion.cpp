#include "aion/gameserver/model/team/legion/Legion.h"

#include <chrono>

#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionHistoryActionInfo.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ICON_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::team::legion {

using configs::main::LegionConfig;
using gameobjects::player::Player;
using HistoryList = runtime::RcArrayList<runtime::Ref<LegionHistoryEntry>>;

Legion::Announcement::Announcement(std::string_view message, commons::database::Timestamp time) : message_(std::string(message)), time_(time) {
}

Legion::Announcement::~Announcement() = default;

runtime::Ref<Legion::Announcement> Legion::Announcement::create(std::string_view message, commons::database::Timestamp time) {
	return runtime::makeRef<Announcement>(message, time);
}

bool Legion::Announcement::equals(const Announcement& obj) const {
	return this == &obj || (message_ == obj.message_ && time_ == obj.time_);
}

int32_t Legion::Announcement::hashCode() const {
	// Java record hashCode: 31 * h + hash(component); String.hashCode over UTF-16 code units, Timestamp.hashCode = Date.hashCode of getTime()
	uint32_t stringHash = 0;
	for (char16_t c : commons::utils::StringUtils::toUtf16(message_))
		stringHash = 31 * stringHash + c;
	const int64_t millis = time_.time_since_epoch().count();
	const uint32_t timeHash = static_cast<uint32_t>(millis) ^ static_cast<uint32_t>(static_cast<uint64_t>(millis) >> 32);
	uint32_t h = stringHash;
	h = 31 * h + timeHash;
	return static_cast<int32_t>(h);
}

Legion::Legion(int32_t legionId, std::string_view legionNameValue)
	: AionObject(legionId), memberIds(runtime::RcCopyOnWriteArrayList<int32_t>::create(AION_LOCK_CLASS(Legion::memberIds))),
	  legionEmblem(LegionEmblem::create()), legionWarehouse(std::make_unique<LegionWarehouse>(*this)) {
	legionName.set(std::string(legionNameValue));
	setHistory({}); // Java: setHistory(Collections.emptyMap())
}

Legion::~Legion() = default;

runtime::Ref<Legion> Legion::create(int32_t legionId, std::string_view legionNameValue) {
	return runtime::makeRef<Legion>(legionId, legionNameValue);
}

void Legion::setMemberIds(const std::vector<int32_t>& value) {
	// Java stores the caller's list; the C++ setter copies the ids into a new list (Legion.h)
	runtime::Ref<runtime::RcCopyOnWriteArrayList<int32_t>> ids = runtime::RcCopyOnWriteArrayList<int32_t>::create(AION_LOCK_CLASS(Legion::memberIds));
	ids->addAll(value);
	this->memberIds.set(std::move(ids));
}

std::vector<runtime::Ptr<LegionMember>> Legion::getMembers() {
	return streamMembers();
}

std::vector<runtime::Ptr<LegionMember>> Legion::streamMembers() {
	// Java: memberIds.stream().map(LegionService.getInstance()::getLegionMember).filter(Objects::nonNull)
	std::vector<runtime::Ptr<LegionMember>> members;
	for (int32_t memberId : *memberIds.get()) {
		runtime::Ptr<LegionMember> member = services::LegionService::getInstance().getLegionMember(memberId);
		if (member)
			members.push_back(member);
	}
	return members;
}

std::vector<runtime::Ptr<Player>> Legion::getOnlinePlayers() {
	// Java: memberIds.stream().map(World.getInstance()::getPlayer).filter(Objects::nonNull).collect(Collectors.toList())
	std::vector<runtime::Ptr<Player>> players;
	for (int32_t memberId : *memberIds.get()) {
		runtime::Ptr<Player> player = world::World::getInstance().getPlayer(memberId);
		if (player)
			players.push_back(player);
	}
	return players;
}

runtime::Ptr<LegionMember> Legion::getBrigadeGeneral() {
	// Java: streamMembers().filter(LegionMember::isBrigadeGeneral).findFirst().orElseThrow() - the stream is lazy: the members after the
	// brigade general are not looked up
	for (int32_t memberId : *memberIds.get()) {
		runtime::Ptr<LegionMember> member = services::LegionService::getInstance().getLegionMember(memberId);
		if (member && member->isBrigadeGeneral())
			return member;
	}
	throw runtime::NoSuchElementException("No value present");
}

bool Legion::addLegionMember(int32_t playerObjId) {
	if (canAddMember()) {
		memberIds.get()->add(playerObjId);
		return true;
	}
	return false;
}

void Legion::removeMember(int32_t playerObjId) {
	memberIds.get()->remove(playerObjId); // Java: remove((Integer) playerObjId) - by value
}

void Legion::setLegionPermissions(int16_t deputyPermissionValue, int16_t centurionPermissionValue, int16_t legionaryPermissionValue,
	int16_t volunteerPermissionValue) {
	this->deputyPermission.set(deputyPermissionValue);
	this->centurionPermission.set(centurionPermissionValue);
	this->legionaryPermission.set(legionaryPermissionValue);
	this->volunteerPermission.set(volunteerPermissionValue);
}

void Legion::setLegionLevel(int32_t value) {
	this->legionLevel.set(value);
	getLegionWarehouse().updateLimit(getWarehouseExpansions());
}

void Legion::addContributionPoints(int64_t value) {
	// java-race: unsynchronized read-modify-write of contributionPoints (Java `this.contributionPoints += contributionPoints`)
	this->contributionPoints.set(this->contributionPoints.get() + value);
}

bool Legion::hasRequiredMembers() {
	int32_t memberSize = getMemberIds()->size();
	switch (getLegionLevel()) {
		case 1:
			return memberSize >= LegionConfig::LEGION_LEVEL2_REQUIRED_MEMBERS.load();
		case 2:
			return memberSize >= LegionConfig::LEGION_LEVEL3_REQUIRED_MEMBERS.load();
		case 3:
			return memberSize >= LegionConfig::LEGION_LEVEL4_REQUIRED_MEMBERS.load();
		case 4:
			return memberSize >= LegionConfig::LEGION_LEVEL5_REQUIRED_MEMBERS.load();
		case 5:
			return memberSize >= LegionConfig::LEGION_LEVEL6_REQUIRED_MEMBERS.load();
		case 6:
			return memberSize >= LegionConfig::LEGION_LEVEL7_REQUIRED_MEMBERS.load();
		case 7:
			return memberSize >= LegionConfig::LEGION_LEVEL8_REQUIRED_MEMBERS.load();
	}
	return false;
}

int32_t Legion::getKinahPrice() {
	switch (getLegionLevel()) {
		case 1:
			return LegionConfig::LEGION_LEVEL2_REQUIRED_KINAH.load();
		case 2:
			return LegionConfig::LEGION_LEVEL3_REQUIRED_KINAH.load();
		case 3:
			return LegionConfig::LEGION_LEVEL4_REQUIRED_KINAH.load();
		case 4:
			return LegionConfig::LEGION_LEVEL5_REQUIRED_KINAH.load();
		case 5:
			return LegionConfig::LEGION_LEVEL6_REQUIRED_KINAH.load();
		case 6:
			return LegionConfig::LEGION_LEVEL7_REQUIRED_KINAH.load();
		case 7:
			return LegionConfig::LEGION_LEVEL8_REQUIRED_KINAH.load();
	}
	return 0;
}

int32_t Legion::getContributionPrice() {
	switch (getLegionLevel()) {
		case 1:
			return LegionConfig::LEGION_LEVEL2_REQUIRED_CONTRIBUTION.load();
		case 2:
			return LegionConfig::LEGION_LEVEL3_REQUIRED_CONTRIBUTION.load();
		case 3:
			return LegionConfig::LEGION_LEVEL4_REQUIRED_CONTRIBUTION.load();
		case 4:
			return LegionConfig::LEGION_LEVEL5_REQUIRED_CONTRIBUTION.load();
		case 5:
			return LegionConfig::LEGION_LEVEL6_REQUIRED_CONTRIBUTION.load();
		case 6:
			return LegionConfig::LEGION_LEVEL7_REQUIRED_CONTRIBUTION.load();
		case 7:
			return LegionConfig::LEGION_LEVEL8_REQUIRED_CONTRIBUTION.load();
	}
	return 0;
}

bool Legion::canAddMember() {
	int32_t memberSize = getMemberIds()->size();
	switch (getLegionLevel()) {
		case 1:
			return memberSize < LegionConfig::LEGION_LEVEL1_MAX_MEMBERS.load();
		case 2:
			return memberSize < LegionConfig::LEGION_LEVEL2_MAX_MEMBERS.load();
		case 3:
			return memberSize < LegionConfig::LEGION_LEVEL3_MAX_MEMBERS.load();
		case 4:
			return memberSize < LegionConfig::LEGION_LEVEL4_MAX_MEMBERS.load();
		case 5:
			return memberSize < LegionConfig::LEGION_LEVEL5_MAX_MEMBERS.load();
		case 6:
			return memberSize < LegionConfig::LEGION_LEVEL6_MAX_MEMBERS.load();
		case 7:
			return memberSize < LegionConfig::LEGION_LEVEL7_MAX_MEMBERS.load();
		case 8:
			return memberSize < LegionConfig::LEGION_LEVEL8_MAX_MEMBERS.load();
	}
	return false;
}

void Legion::setAnnouncement(runtime::Ptr<Legion::Announcement> value) {
	this->announcement.set(runtime::Ref<Legion::Announcement>(value));
}

bool Legion::isDisbanding() {
	return disbandTime.get() > 0;
}

bool Legion::isMember(int32_t playerObjId) {
	return memberIds.get()->contains(playerObjId);
}

void Legion::setLegionEmblem(runtime::Ptr<LegionEmblem> value) {
	this->legionEmblem.set(runtime::Ref<LegionEmblem>(value));
}

LegionWarehouse& Legion::getLegionWarehouse() const {
	return *legionWarehouse;
}

int32_t Legion::getWarehouseExpansions() {
	return getLegionLevel() - 1;
}

std::vector<runtime::Ptr<LegionHistoryEntry>> Legion::getHistory(LegionHistoryAction_Type type) {
	runtime::Ptr<HistoryList> history = legionHistoryByType.get(type);
	SYNCHRONIZED(*history) {
		// Java: new ArrayList<>(history)
		std::vector<runtime::Ptr<LegionHistoryEntry>> copy;
		for (const runtime::Ref<LegionHistoryEntry>& entry : history->snapshot())
			copy.push_back(entry);
		return copy;
	}
}

std::vector<runtime::Ref<LegionHistoryEntry>> Legion::addHistory(LegionHistoryEntry& entry) {
	std::vector<runtime::Ref<LegionHistoryEntry>> removedEntries;
	LegionHistoryAction_Type type = getType(entry.action());
	runtime::Ptr<HistoryList> history = legionHistoryByType.get(type);
	SYNCHRONIZED(*history) {
		history->add(0, runtime::Ref<LegionHistoryEntry>(entry)); // Java: history.addFirst(entry) (List.addFirst = add(0, e))
		if (type == LegionHistoryAction_Type::REWARD || type == LegionHistoryAction_Type::WAREHOUSE) {
			int64_t maxMillis = commons::utils::currentTimeMillis() / 1000 - std::chrono::duration_cast<std::chrono::seconds>(std::chrono::days(365)).count();
			// Java: history.getLast() / history.removeLast() (List defaults: get(size() - 1) / remove(size() - 1)); the list holds the new entry
			while (history->get(history->size() - 1)->epochSeconds() < maxMillis)
				removedEntries.push_back(runtime::Ref<LegionHistoryEntry>(history->removeAt(history->size() - 1)));
		}
	}
	return removedEntries;
}

void Legion::setHistory(const std::map<LegionHistoryAction_Type, std::vector<runtime::Ref<LegionHistoryEntry>>>& history) {
	for (LegionHistoryAction_Type type : {LegionHistoryAction_Type::LEGION, LegionHistoryAction_Type::REWARD, LegionHistoryAction_Type::WAREHOUSE}) {
		auto entries = history.find(type);
		// Java: entries == null ? new ArrayList<>(1) : entries - the C++ signature passes vectors, so a found list is copied into a new one
		runtime::Ref<HistoryList> list = HistoryList::create(AION_LOCK_CLASS(Legion::history));
		if (entries != history.end())
			for (const runtime::Ref<LegionHistoryEntry>& entry : entries->second)
				list->add(entry);
		legionHistoryByType.put(type, std::move(list));
	}
}

void Legion::addBonus() {
	std::vector<runtime::Ptr<Player>> members = getOnlinePlayers();
	if (members.size() >= 10) {
		if (hasBonus_.compareAndSet(false, true)) {
			for (const runtime::Ptr<Player>& member : members) {
				utils::PacketSendUtility::sendPacket(*member, network::aion::serverpackets::SM_ICON_INFO(1, true));
			}
		}
	}
}

void Legion::removeBonus() {
	std::vector<runtime::Ptr<Player>> members = getOnlinePlayers();
	if (members.size() < 10) {
		if (hasBonus_.compareAndSet(true, false)) {
			for (const runtime::Ptr<Player>& member : members) {
				utils::PacketSendUtility::sendPacket(*member, network::aion::serverpackets::SM_ICON_INFO(1, false));
			}
		}
	}
}

bool Legion::hasBonus() {
	return hasBonus_.get();
}

std::string Legion::toString() {
	return "Legion [id=" + std::to_string(getObjectId()) + ", name=" + getName() + "]";
}

} // namespace aion::gameserver::model::team::legion
