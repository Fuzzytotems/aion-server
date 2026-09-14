#include "aion/gameserver/network/aion/serverpackets/SM_FIND_GROUP.h"

#include "aion/gameserver/model/gameobjects/findGroup/FindGroupEntry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FIND_GROUP::SM_FIND_GROUP(int32_t actionValue, const std::vector<runtime::Ptr<model::gameobjects::findGroup::FindGroupEntry>>& entriesValue)
	: AionServerPacket(opcodeOf<SM_FIND_GROUP>), action(actionValue), entries(entriesValue.begin(), entriesValue.end()) {
}

SM_FIND_GROUP::SM_FIND_GROUP(int32_t recruitmentIdToDelete, int8_t serverIdValue, int8_t unk1Value, int8_t unk2Value, int8_t unk3Value)
	: AionServerPacket(opcodeOf<SM_FIND_GROUP>), action(1), serverId(serverIdValue), unk1(unk1Value), unk2(unk2Value), unk3(unk3Value),
	  idToDelete(recruitmentIdToDelete) {
}

SM_FIND_GROUP::SM_FIND_GROUP(int32_t applicationIdToDelete)
	: AionServerPacket(opcodeOf<SM_FIND_GROUP>), action(5), idToDelete(applicationIdToDelete) {
}

SM_FIND_GROUP::SM_FIND_GROUP(model::gameobjects::player::Player& instanceApplicantValue)
	: AionServerPacket(opcodeOf<SM_FIND_GROUP>), action(11), instanceApplicant(instanceApplicantValue) {
}

SM_FIND_GROUP::SM_FIND_GROUP(bool showEnterInstanceMessageValue)
	: AionServerPacket(opcodeOf<SM_FIND_GROUP>), action(23), showEnterInstanceMessage(showEnterInstanceMessageValue) {
}

SM_FIND_GROUP::SM_FIND_GROUP(const std::vector<int32_t>& instanceMaskIdsValue)
	: AionServerPacket(opcodeOf<SM_FIND_GROUP>), action(26), instanceMaskIds(instanceMaskIdsValue) {
}

SM_FIND_GROUP::~SM_FIND_GROUP() = default;

void SM_FIND_GROUP::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::showRecruitments(const std::vector<runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment>>& recruitments,
	int32_t lastUpdate) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::removeRecruitment(int32_t playerOrTeamId, int8_t value, int8_t unk1Value, int8_t unk2Value, int8_t unk3Value) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::showApplications(const std::vector<runtime::Ptr<model::gameobjects::findGroup::GroupApplication>>& applications,
	int32_t lastUpdate) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::removeApplication(int32_t playerId) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::showInstanceGroups(const std::vector<runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup>>& instanceGroups,
	int32_t lastUpdate) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::sendInstanceGroupApplicationAsWhisperChatMessage(model::gameobjects::player::Player& value) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::registerInstanceGroup(const std::vector<runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup>>& instanceGroups) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::showInstanceGroupMemberInfo(model::gameobjects::findGroup::ServerWideGroup& instanceGroup, int32_t lastUpdate) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::showEnterButtonInPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::showPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::destroyPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup, bool value) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::updatePrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup) {
	AION_UNPORTED();
}

void SM_FIND_GROUP::enableRegisterForInstances(const std::vector<int32_t>& value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
