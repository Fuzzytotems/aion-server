#include "aion/gameserver/services/LegionService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/Legion.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.LegionService");

LegionService::LegionService() : legionRestrictions(LegionRestrictions::create()) {
}

LegionService::~LegionService() = default;

LegionService& LegionService::getInstance() {
	static LegionService instance; // Java SingletonHolder
	return instance;
}

runtime::Ref<LegionService::LegionRestrictions> LegionService::LegionRestrictions::create() {
	return runtime::makeRef<LegionRestrictions>();
}

bool LegionService::LegionRestrictions::canCreateLegion(model::gameobjects::player::Player& activePlayer, std::string_view legionName) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canInvitePlayer(model::gameobjects::player::Player& activePlayer, runtime::Ptr<model::gameobjects::player::Player> targetPlayer) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canKickPlayer(model::gameobjects::player::Player& player, std::string_view charName, runtime::Ptr<model::team::legion::LegionMember> legionMember) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canAppointBrigadeGeneral(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& targetPlayer) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canAppointRank(model::gameobjects::player::Player& activePlayer, runtime::Ptr<model::team::legion::LegionMember> targetMember) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canChangeSelfIntro(model::gameobjects::player::Player& activePlayer, std::string_view newSelfIntro) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canChangeLevel(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canChangeNickname(model::gameobjects::player::Player& player, runtime::Ptr<model::team::legion::LegionMember> member, std::string_view memberName, std::string_view newNickname) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canDisbandLegion(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canLeave(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canRecreateLegion(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canUploadEmblem(model::gameobjects::player::Player& activePlayer, bool initUpload) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canOpenWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::canStoreLegionEmblem(model::gameobjects::player::Player& activePlayer, int32_t emblemId) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::isBrigadeGeneral(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::isFreeName(std::string_view name) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::isValidSelfIntro(std::string_view name) {
	AION_UNPORTED();
}

bool LegionService::LegionRestrictions::isValidNickname(std::string_view name) {
	AION_UNPORTED();
}

LegionService::LegionRestrictions::~LegionRestrictions() = default;

void LegionService::storeLegion(model::team::legion::Legion& legion, bool newLegion) {
	AION_UNPORTED();
}

void LegionService::storeLegion(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

void LegionService::storeLegionMember(model::team::legion::LegionMember& legionMember) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::team::legion::Legion>> LegionService::getCachedLegions() {
	AION_UNPORTED();
}

void LegionService::addCachedLegion(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

void LegionService::deleteLegionFromDB(int32_t legionId) {
	AION_UNPORTED();
}

void LegionService::deleteLegionMemberFromDB(model::team::legion::LegionMember& legionMember) {
	AION_UNPORTED();
}

runtime::Ptr<model::team::legion::Legion> LegionService::getLegion(std::string_view legionName) {
	AION_UNPORTED();
}

runtime::Ptr<model::team::legion::Legion> LegionService::getLegion(int32_t legionId) {
	AION_UNPORTED();
}

void LegionService::loadLegionInfo(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(std::string_view name) {
	AION_UNPORTED();
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(int32_t playerObjId) {
	AION_UNPORTED();
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(model::gameobjects::player::PlayerCommonData& playerCommonData) {
	AION_UNPORTED();
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(int32_t playerObjectId, runtime::Ptr<model::gameobjects::player::PlayerCommonData> playerCommonData) {
	AION_UNPORTED();
}

bool LegionService::checkDisband(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

void LegionService::disbandLegion(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at LegionService.java:191 (fieldmap key LegionService$1); local disbandResponseHandler; storage: stored in ResponseRequester
void LegionService::requestDisbandLegion(model::gameobjects::Npc& npc, model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

void LegionService::createLegion(model::gameobjects::player::Player& activePlayer, std::string_view legionName) {
	AION_UNPORTED();
}

bool LegionService::addToLegion(model::team::legion::Legion& legion, model::gameobjects::player::Player& invited, model::gameobjects::player::Player& inviter) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at LegionService.java:246 (fieldmap key LegionService$2); local responseHandler; storage: stored in ResponseRequester
void LegionService::invitePlayerToLegion(model::gameobjects::player::Player& activePlayer, std::string_view targetName) {
	AION_UNPORTED();
}

void LegionService::displayLegionAnnouncement(model::gameobjects::player::Player& targetPlayer, runtime::Ptr<model::team::legion::Legion::Announcement> announcement) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at LegionService.java:288 (fieldmap key LegionService$3); local responseHandler; storage: stored in ResponseRequester
void LegionService::startBrigadeGeneralChangeProcess(model::gameobjects::player::Player& legionLeader, std::string_view memberName) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at LegionService.java:303 (fieldmap key LegionService$4); local responseHandler; storage: stored in ResponseRequester
void LegionService::appointBrigadeGeneral(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& targetPlayer) {
	AION_UNPORTED();
}

void LegionService::appointBrigadeGeneral(model::team::legion::LegionMember& member) {
	AION_UNPORTED();
}

void LegionService::appointRank(model::gameobjects::player::Player& player, std::string_view charName, int32_t rankId) {
	AION_UNPORTED();
}

void LegionService::changeSelfIntro(model::gameobjects::player::Player& activePlayer, std::string_view newSelfIntro) {
	AION_UNPORTED();
}

void LegionService::changePermissions(model::gameobjects::player::Player& player, int16_t deputyPermission, int16_t centurionPermission, int16_t legionarPermission, int16_t volunteerPermission) {
	AION_UNPORTED();
}

void LegionService::requestChangeLevel(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

void LegionService::changeLevel(model::team::legion::Legion& legion, int32_t newLevel, bool save) {
	AION_UNPORTED();
}

void LegionService::changeNickname(model::gameobjects::player::Player& activePlayer, std::string_view memberName, std::string_view newNickname) {
	AION_UNPORTED();
}

void LegionService::updateAfterDisbandLegion(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

void LegionService::updateMembersEmblem(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

void LegionService::updateMembersOfDisbandLegion(model::team::legion::Legion& legion, int32_t unixTime) {
	AION_UNPORTED();
}

void LegionService::updateMembersOfRecreateLegion(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

void LegionService::storeLegionEmblem(model::gameobjects::player::Player& activePlayer, int32_t emblemId, int32_t color_a, int32_t color_r, int32_t color_g, int32_t color_b, model::team::legion::LegionEmblemType emblemType) {
	AION_UNPORTED();
}

void LegionService::openLegionWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at LegionService.java:498 (fieldmap key LegionService$5); local disbandResponseHandler; storage: stored in ResponseRequester
void LegionService::recreateLegion(model::gameobjects::Npc& npc, model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

void LegionService::LegionWhUpdate(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void LegionService::updateMemberInfo(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void LegionService::setContributionPoints(model::team::legion::Legion& legion, int64_t newPoints, bool save) {
	AION_UNPORTED();
}

void LegionService::uploadEmblemInfo(model::gameobjects::player::Player& activePlayer, int32_t totalSize, int32_t color_a, int32_t color_r, int32_t color_g, int32_t color_b, model::team::legion::LegionEmblemType emblemType) {
	AION_UNPORTED();
}

void LegionService::uploadEmblemData(model::gameobjects::player::Player& activePlayer, int32_t size, std::span<const uint8_t> data) {
	AION_UNPORTED();
}

void LegionService::sendEmblemData(model::gameobjects::player::Player& player, model::team::legion::LegionEmblem& legionEmblem, int32_t legionId, std::string_view legionName) {
	AION_UNPORTED();
}

void LegionService::changeAnnouncement(model::gameobjects::player::Player& activePlayer, std::string_view message) {
	AION_UNPORTED();
}

void LegionService::addHistory(model::team::legion::Legion& legion, std::string_view text, model::team::legion::LegionHistoryAction action) {
	AION_UNPORTED();
}

void LegionService::addRewardHistory(model::team::legion::Legion& legion, int64_t kinahAmount, model::team::legion::LegionHistoryAction action, int32_t fortressId) {
	AION_UNPORTED();
}

void LegionService::addHistory(model::team::legion::Legion& legion, std::string_view name, model::team::legion::LegionHistoryAction action, std::string_view description) {
	AION_UNPORTED();
}

void LegionService::addLegionMember(model::team::legion::Legion& legion, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void LegionService::addLegionMember(model::team::legion::Legion& legion, model::gameobjects::player::Player& player, model::team::legion::LegionRank rank) {
	AION_UNPORTED();
}

bool LegionService::removeLegionMember(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool LegionService::removeLegionMember(runtime::Ptr<model::team::legion::LegionMember> legionMember, std::optional<std::string_view> kickerName) {
	AION_UNPORTED();
}

void LegionService::kickMember(model::gameobjects::player::Player& player, std::string_view memberName) {
	AION_UNPORTED();
}

bool LegionService::leaveLegion(model::gameobjects::player::Player& player, bool skipChecks) {
	AION_UNPORTED();
}

void LegionService::onLogin(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

void LegionService::onLogout(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void LegionService::addWHItemHistory(model::gameobjects::player::Player& player, int32_t itemId, int64_t value, model::items::storage::IStorage& sourceStorage, model::items::storage::IStorage& destStorage) {
	AION_UNPORTED();
}

void LegionService::updateLegionMemberList(model::gameobjects::player::Player& player, bool broadcastToLegion) {
	AION_UNPORTED();
}

void LegionService::updateLegionMemberList(runtime::Ptr<model::gameobjects::player::Player> player, bool broadcastToLegion, std::optional<int32_t> excludedPlayerId) {
	AION_UNPORTED();
}

bool LegionService::tryRename(model::team::legion::Legion& legion, std::string_view name, model::gameobjects::player::Player& player, std::optional<int32_t> legionNameChangeTicketItemObjId) {
	AION_UNPORTED();
}

void LegionService::joinLegionDominion(model::gameobjects::player::Player& player, int32_t locId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
