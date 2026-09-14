#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

PlayerCommonData::PlayerCommonData(int32_t objId) : playerObjId(objId) {
}

PlayerCommonData::~PlayerCommonData() = default;

runtime::Ref<PlayerCommonData> PlayerCommonData::create(int32_t objId) {
	return runtime::makeRef<PlayerCommonData>(objId);
}

int64_t PlayerCommonData::getExpShown() {
	AION_UNPORTED();
}

int64_t PlayerCommonData::getExpNeed() {
	AION_UNPORTED();
}

void PlayerCommonData::calculateExpLoss() {
	AION_UNPORTED();
}

void PlayerCommonData::resetRecoverableExp() {
	AION_UNPORTED();
}

void PlayerCommonData::addExp(int64_t value, Rates rates) {
	AION_UNPORTED();
}

void PlayerCommonData::addExp(int64_t value, Rates rates, std::optional<std::string_view> nameValue) {
	AION_UNPORTED();
}

bool PlayerCommonData::isReadyForSalvationPoints() {
	AION_UNPORTED();
}

bool PlayerCommonData::isReadyForReposeEnergy() {
	AION_UNPORTED();
}

void PlayerCommonData::addReposeEnergy(int64_t add) {
	AION_UNPORTED();
}

void PlayerCommonData::updateMaxRepose() {
	AION_UNPORTED();
}

void PlayerCommonData::setExp(int64_t expValue) {
	AION_UNPORTED();
}

bool PlayerCommonData::isHaveMentorFlag() {
	AION_UNPORTED();
}

int32_t PlayerCommonData::getLastOnlineEpochSeconds() {
	AION_UNPORTED();
}

void PlayerCommonData::setLevel(int32_t levelValue) {
	AION_UNPORTED();
}

runtime::Ptr<Player> PlayerCommonData::getPlayer() {
	AION_UNPORTED();
}

void PlayerCommonData::addDp(int32_t dpValue) {
	AION_UNPORTED();
}

void PlayerCommonData::setDp(int32_t dpValue) {
	AION_UNPORTED();
}

int32_t PlayerCommonData::getTemplateId() const {
	AION_UNPORTED();
}

int8_t PlayerCommonData::getCurrentSalvationPercent() {
	AION_UNPORTED();
}

void PlayerCommonData::addSalvationPoints(int64_t points) {
	AION_UNPORTED();
}

void PlayerCommonData::resetSalvationPoints() {
	AION_UNPORTED();
}

bool PlayerCommonData::updateDaeva() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
