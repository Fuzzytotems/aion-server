#include "aion/gameserver/network/aion/serverpackets/SM_FIND_GROUP.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/findGroup/FindGroupEntry.h"
#include "aion/gameserver/model/gameobjects/findGroup/GroupApplication.h"
#include "aion/gameserver/model/gameobjects/findGroup/GroupRecruitment.h"
#include "aion/gameserver/model/gameobjects/findGroup/ServerWideGroup.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: the unchecked cast (List<T>) entries - elements of another type throw ClassCastException when used */
template <class T>
std::vector<runtime::Ptr<T>> castEntries(const std::vector<runtime::Ref<model::gameobjects::findGroup::FindGroupEntry>>& entries) {
	std::vector<runtime::Ptr<T>> result;
	result.reserve(entries.size());
	for (const runtime::Ref<model::gameobjects::findGroup::FindGroupEntry>& entry : entries)
		result.push_back(runtime::cast<T>(entry));
	return result;
}

/** Java: (ServerWideGroup) entries.get(0) */
model::gameobjects::findGroup::ServerWideGroup& firstServerWideGroup(
	const std::vector<runtime::Ref<model::gameobjects::findGroup::FindGroupEntry>>& entries) {
	if (entries.empty())
		throw commons::utils::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return *runtime::cast<model::gameobjects::findGroup::ServerWideGroup>(entries.front());
}

/** Java: (int) (System.currentTimeMillis() / 1000) */
int32_t nowSeconds() {
	return static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
}

} // namespace

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
	using model::gameobjects::findGroup::GroupApplication;
	using model::gameobjects::findGroup::GroupRecruitment;
	using model::gameobjects::findGroup::ServerWideGroup;
	writeC(action);
	switch (action) {
		case 0:
			showRecruitments(castEntries<GroupRecruitment>(entries), nowSeconds());
			break;
		case 1:
			removeRecruitment(idToDelete, serverId, unk1, unk2, unk3);
			break;
		case 4:
			showApplications(castEntries<GroupApplication>(entries), nowSeconds());
			break;
		case 5:
			removeApplication(idToDelete);
			break;
		case 10:
			showInstanceGroups(castEntries<ServerWideGroup>(entries), nowSeconds());
			break;
		case 11:
			sendInstanceGroupApplicationAsWhisperChatMessage(*runtime::Ptr<model::gameobjects::player::Player>(instanceApplicant));
			break;
		case 14:
			registerInstanceGroup(castEntries<ServerWideGroup>(entries));
			break;
		case 16:
			showInstanceGroupMemberInfo(firstServerWideGroup(entries), nowSeconds());
			break;
		case 18:
			showEnterButtonInPrepareForEntryWindow(firstServerWideGroup(entries)); // window must be initialized
			break;
		case 22:
			showPrepareForEntryWindow(firstServerWideGroup(entries)); // initialize window if necessary
			break;
		case 23:
			destroyPrepareForEntryWindow(firstServerWideGroup(entries), showEnterInstanceMessage);
			break;
		case 24:
			updatePrepareForEntryWindow(firstServerWideGroup(entries));
			break;
		case 26:
			enableRegisterForInstances(instanceMaskIds);
			break;
	}
}

void SM_FIND_GROUP::showRecruitments(const std::vector<runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment>>& recruitments,
	int32_t lastUpdate) {
	writeH(static_cast<int32_t>(recruitments.size()));
	writeH(static_cast<int32_t>(recruitments.size()));
	writeD(lastUpdate);
	for (runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment> recruitment : recruitments) {
		writeD(recruitment->getObjectId()); // team ID or recruiter ID if still solo
		writeC(configs::network::NetworkConfig::GAMESERVER_ID.load());
		writeC(0); // unk (always 0)
		writeC(0); // unk (always 0)
		writeC(runtime::as<model::gameobjects::player::Player>(recruitment->getObject()) ? 16 : 0); // 16: solo, 0: group | alliance
		writeC(recruitment->getGroupType()); // 0: group, 1: alliance, 2: mentor
		writeS(recruitment->getMessage()); // text
		writeS(recruitment->getName()); // recruiter name
		writeC(recruitment->getSize()); // members count
		writeC(recruitment->getMinLevel()); // members lowest level
		writeC(recruitment->getMaxLevel()); // members highest level
		writeD(recruitment->getLastUpdate()); // client hides entries older than two hours
	}
}

void SM_FIND_GROUP::removeRecruitment(int32_t playerOrTeamId, int8_t value, int8_t unk1Value, int8_t unk2Value, int8_t unk3Value) {
	writeD(playerOrTeamId);
	writeC(value); // serverId
	writeC(unk1Value); // unk (always 0)
	writeC(unk2Value); // unk (always 0)
	writeC(unk3Value); // 16: solo, 0: group | alliance
}

void SM_FIND_GROUP::showApplications(const std::vector<runtime::Ptr<model::gameobjects::findGroup::GroupApplication>>& applications,
	int32_t lastUpdate) {
	writeH(static_cast<int32_t>(applications.size()));
	writeH(static_cast<int32_t>(applications.size()));
	writeD(lastUpdate);
	for (runtime::Ptr<model::gameobjects::findGroup::GroupApplication> application : applications) {
		writeD(application->getPlayer()->getObjectId());
		writeC(application->getGroupType()); // 0:group, 1:alliance
		writeS(application->getMessage()); // text
		writeS(application->getPlayer()->getName(true));
		writeC(application->getClassId()); // applied player class id
		writeC(application->getLevel()); // applied player level
		writeD(application->getLastUpdate()); // client hides entries older than two hours
	}
}

void SM_FIND_GROUP::removeApplication(int32_t playerId) {
	writeD(playerId);
}

void SM_FIND_GROUP::showInstanceGroups(const std::vector<runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup>>& instanceGroups,
	int32_t lastUpdate) {
	writeH(static_cast<int32_t>(instanceGroups.size()));
	writeH(static_cast<int32_t>(instanceGroups.size()));
	writeD(lastUpdate);
	for (runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup> instanceGroup : instanceGroups) {
		writeD(instanceGroup->getId()); // GroupEntryId
		writeD(instanceGroup->getInstanceMaskId());
		writeD(1); // unk
		writeC(static_cast<int32_t>(instanceGroup->getMembers().size()));
		writeC(instanceGroup->getMinMembers());
		writeH(0); // unk maybe spacer
		writeD(instanceGroup->getRecruiter()->getObjectId()); // playerObjId
		writeD(1); // unk
		writeD(0); // unk
		writeC(instanceGroup->getMinLevel()); // playerLevel
		writeC(instanceGroup->getMaxLevel()); // playerLevel
		writeH(0); // unk maybe spacer?
		writeD(instanceGroup->getLastUpdate()); // lastUpdate
		writeD(0); // unk
		writeS(instanceGroup->getRecruiter()->getName(true));
		writeS(instanceGroup->getMessage()); // Message
	}
}

void SM_FIND_GROUP::sendInstanceGroupApplicationAsWhisperChatMessage(model::gameobjects::player::Player& value) {
	writeD(value.getObjectId());
	writeD(0);
	writeD(0);
	writeH(0);
	writeC(0);
	writeC(model::getClassId(value.getPlayerClass()));
	writeD(value.getLevel());
	writeS(value.getName(true));
}

void SM_FIND_GROUP::registerInstanceGroup(const std::vector<runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup>>& instanceGroups) {
	writeC(1); // packetNumber 0 || 1 || 2
	for (runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup> instanceGroup : instanceGroups) {
		writeD(instanceGroup->getId()); // GroupEntryId (counts forwards every entry)
		writeD(instanceGroup->getInstanceMaskId());
		writeD(1); // position?
		writeC(static_cast<int32_t>(instanceGroup->getMembers().size()));
		writeC(instanceGroup->getMinMembers()); // min members to enter Instance(writer choose it)
		writeH(0); // unk maybe spacer
		writeD(instanceGroup->getRecruiter()->getObjectId()); // playerObjId leader ID?
		writeC(1); // unk
		writeC(0); // unkGroupType?
		writeD(1); // unk
		writeH(0); // unk
		writeC(instanceGroup->getMinLevel());
		writeC(instanceGroup->getMaxLevel());
		writeH(0); // unk
		writeD(instanceGroup->getLastUpdate()); // timestamp
		writeD(0); // unk
		writeS(instanceGroup->getRecruiter()->getName(true));
		writeS(instanceGroup->getMessage());
	}
}

void SM_FIND_GROUP::showInstanceGroupMemberInfo(model::gameobjects::findGroup::ServerWideGroup& instanceGroup, int32_t lastUpdate) {
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> members = instanceGroup.getMembers();
	writeH(static_cast<int32_t>(members.size()));
	writeH(static_cast<int32_t>(members.size()));
	writeD(lastUpdate);
	for (runtime::Ptr<model::gameobjects::player::Player> member : members) {
		writeD(0); // groupId?
		writeD(member->getWorldId());
		writeD(member->getObjectId());
		writeD(member->getLevel());
		writeD(model::getClassId(member->getPlayerClass()));
		writeH(1); // unk
		writeC(0); // groupType?
		writeC(0); // unk
		writeS(member->getName(true));
	}
}

void SM_FIND_GROUP::showEnterButtonInPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup) {
	writeD(instanceGroup.getId()); // GroupEntryId
	writeD(instanceGroup.getInstanceMaskId());
}

void SM_FIND_GROUP::showPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup) {
	writeD(instanceGroup.getId()); // GroupEntryId
	writeD(instanceGroup.getInstanceMaskId());
}

void SM_FIND_GROUP::destroyPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup, bool value) {
	writeD(instanceGroup.getId()); // GroupEntryId
	writeD(instanceGroup.getInstanceMaskId());
	writeC(value ? 1 : 0);
}

void SM_FIND_GROUP::updatePrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup) {
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> instanceGroupMembers = instanceGroup.getMembers();
	writeD(instanceGroup.getId()); // GroupEntryId
	writeD(instanceGroup.getInstanceMaskId());
	writeC(static_cast<int32_t>(instanceGroupMembers.size()));
	for (runtime::Ptr<model::gameobjects::player::Player> member : instanceGroupMembers) {
		writeD(0); // server ID?
		writeD(0); // server ID?
		writeD(member->getObjectId());
		writeD(member->getLevel());
		writeD(model::getClassId(member->getPlayerClass()));
		writeH(0); // ?
		writeC(1); // 0: Preparing, 1: Ready
		writeC(member->isOnline() ? 1 : 0);
		writeS(member->getName(true));
	}
}

void SM_FIND_GROUP::enableRegisterForInstances(const std::vector<int32_t>& value) {
	writeH(static_cast<int32_t>(value.size()));
	for (int32_t instanceMaskId : value)
		writeD(instanceMaskId);
}

} // namespace aion::gameserver::network::aion::serverpackets
