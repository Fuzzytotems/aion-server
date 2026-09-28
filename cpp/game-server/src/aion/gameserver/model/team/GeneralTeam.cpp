#include "aion/gameserver/model/team/GeneralTeam.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/TeamMember.h"

namespace aion::gameserver::model::team {

[[maybe_unused]] static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.team.GeneralTeam");

GeneralTeam::GeneralTeam(int32_t objId, bool autoReleaseObjectId) : AionObject(objId, autoReleaseObjectId) {
}

GeneralTeam::~GeneralTeam() = default;

void GeneralTeam::onEvent(TeamEvent& event) {
	AION_UNPORTED();
}

runtime::Ptr<TeamMember> GeneralTeam::getMember(int32_t objectIdValue) {
	AION_UNPORTED();
}

bool GeneralTeam::hasMember(int32_t objectIdValue) {
	AION_UNPORTED();
}

void GeneralTeam::addMember(TeamMember& member) {
	AION_UNPORTED();
}

runtime::Ptr<TeamMember> GeneralTeam::removeMember(TeamMember& member) {
	AION_UNPORTED();
}

runtime::Ptr<TeamMember> GeneralTeam::removeMember(int32_t objectIdValue) {
	AION_UNPORTED();
}

void GeneralTeam::forEachTeamMember(const std::function<void(TeamMember&)>& consumer) {
	AION_UNPORTED();
}

void GeneralTeam::forEach(const std::function<void(gameobjects::AionObject&)>& consumer) {
	AION_UNPORTED();
}

void GeneralTeam::applyOnMembers(const std::function<bool(gameobjects::AionObject&)>& function) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<TeamMember>> GeneralTeam::filter(const std::function<bool(TeamMember&)>& predicate) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::AionObject>> GeneralTeam::filterMembers(const std::function<bool(gameobjects::AionObject&)>& predicate) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::AionObject>> GeneralTeam::getMembers() {
	AION_UNPORTED();
}

int32_t GeneralTeam::size() {
	AION_UNPORTED();
}

bool GeneralTeam::isDisbanded() {
	AION_UNPORTED();
}

bool GeneralTeam::shouldDisband() {
	AION_UNPORTED();
}

bool GeneralTeam::isFull() {
	AION_UNPORTED();
}

int32_t GeneralTeam::getTeamId() {
	AION_UNPORTED();
}

std::string GeneralTeam::getName() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::AionObject> GeneralTeam::getLeaderObject() {
	AION_UNPORTED();
}

bool GeneralTeam::isLeader(gameobjects::AionObject& member) {
	AION_UNPORTED();
}

void GeneralTeam::changeLeader(TeamMember& member) {
	AION_UNPORTED();
}

void GeneralTeam::setLeader(TeamMember& member) {
	AION_UNPORTED();
}

void GeneralTeam::lock() {
	AION_UNPORTED();
}

void GeneralTeam::unlock() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team
