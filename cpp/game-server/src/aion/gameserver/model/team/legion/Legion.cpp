#include "aion/gameserver/model/team/legion/Legion.h"

#include <chrono>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"

namespace aion::gameserver::model::team::legion {

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
	// Java: setHistory(Collections.emptyMap())
	AION_UNPORTED();
}

Legion::~Legion() = default;

runtime::Ref<Legion> Legion::create(int32_t legionId, std::string_view legionNameValue) {
	return runtime::makeRef<Legion>(legionId, legionNameValue);
}

void Legion::setMemberIds(const std::vector<int32_t>& value) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<LegionMember>> Legion::getMembers() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<LegionMember>> Legion::streamMembers() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::player::Player>> Legion::getOnlinePlayers() {
	AION_UNPORTED();
}

runtime::Ptr<LegionMember> Legion::getBrigadeGeneral() {
	AION_UNPORTED();
}

bool Legion::addLegionMember(int32_t playerObjId) {
	AION_UNPORTED();
}

void Legion::removeMember(int32_t playerObjId) {
	AION_UNPORTED();
}

void Legion::setLegionPermissions(int16_t deputyPermissionValue, int16_t centurionPermissionValue, int16_t legionaryPermissionValue,
	int16_t volunteerPermissionValue) {
	AION_UNPORTED();
}

void Legion::setLegionLevel(int32_t value) {
	AION_UNPORTED();
}

void Legion::addContributionPoints(int64_t value) {
	AION_UNPORTED();
}

bool Legion::hasRequiredMembers() {
	AION_UNPORTED();
}

int32_t Legion::getKinahPrice() {
	AION_UNPORTED();
}

int32_t Legion::getContributionPrice() {
	AION_UNPORTED();
}

bool Legion::canAddMember() {
	AION_UNPORTED();
}

void Legion::setAnnouncement(runtime::Ptr<Legion::Announcement> value) {
	AION_UNPORTED();
}

bool Legion::isDisbanding() {
	AION_UNPORTED();
}

bool Legion::isMember(int32_t playerObjId) {
	AION_UNPORTED();
}

void Legion::setLegionEmblem(runtime::Ptr<LegionEmblem> value) {
	AION_UNPORTED();
}

LegionWarehouse& Legion::getLegionWarehouse() const {
	return *legionWarehouse;
}

int32_t Legion::getWarehouseExpansions() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<LegionHistoryEntry>> Legion::getHistory(LegionHistoryAction_Type type) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<LegionHistoryEntry>> Legion::addHistory(LegionHistoryEntry& entry) {
	AION_UNPORTED();
}

void Legion::setHistory(const std::map<LegionHistoryAction_Type, std::vector<runtime::Ref<LegionHistoryEntry>>>& history) {
	AION_UNPORTED();
}

void Legion::addBonus() {
	AION_UNPORTED();
}

void Legion::removeBonus() {
	AION_UNPORTED();
}

bool Legion::hasBonus() {
	AION_UNPORTED();
}

std::string Legion::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::legion
