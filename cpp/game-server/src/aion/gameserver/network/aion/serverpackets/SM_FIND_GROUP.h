#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/findGroup/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author cura, MrPoke
 */
class SM_FIND_GROUP : public AionServerPacket {
private:
	int32_t action{};
	std::vector<runtime::Ref<model::gameobjects::findGroup::FindGroupEntry>> entries{};
	int8_t serverId{};
	int8_t unk1{};
	int8_t unk2{};
	int8_t unk3{};
	int32_t idToDelete{};
	runtime::Ref<model::gameobjects::player::Player> instanceApplicant{};
	bool showEnterInstanceMessage{};
	std::vector<int32_t> instanceMaskIds{};

public:
	SM_FIND_GROUP(int32_t action, const std::vector<runtime::Ptr<model::gameobjects::findGroup::FindGroupEntry>>& entries);
	SM_FIND_GROUP(int32_t recruitmentIdToDelete, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3);
	explicit SM_FIND_GROUP(int32_t applicationIdToDelete);
	explicit SM_FIND_GROUP(model::gameobjects::player::Player& instanceApplicant);
	explicit SM_FIND_GROUP(bool showEnterInstanceMessage);
	explicit SM_FIND_GROUP(const std::vector<int32_t>& instanceMaskIds);
	~SM_FIND_GROUP() override;

protected:
	void writeImpl(AionConnection* con) override;

private:
	void showRecruitments(const std::vector<runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment>>& recruitments, int32_t lastUpdate);
	void removeRecruitment(int32_t playerOrTeamId, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3);
	void showApplications(const std::vector<runtime::Ptr<model::gameobjects::findGroup::GroupApplication>>& applications, int32_t lastUpdate);
	void removeApplication(int32_t playerId);
	void showInstanceGroups(const std::vector<runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup>>& instanceGroups, int32_t lastUpdate);
	void sendInstanceGroupApplicationAsWhisperChatMessage(model::gameobjects::player::Player& instanceApplicant);
	void registerInstanceGroup(const std::vector<runtime::Ptr<model::gameobjects::findGroup::ServerWideGroup>>& instanceGroups);
	void showInstanceGroupMemberInfo(model::gameobjects::findGroup::ServerWideGroup& instanceGroup, int32_t lastUpdate);
	void showEnterButtonInPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup);
	void showPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup);
	void destroyPrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup, bool showEnterInstanceMessage);
	void updatePrepareForEntryWindow(model::gameobjects::findGroup::ServerWideGroup& instanceGroup);
	void enableRegisterForInstances(const std::vector<int32_t>& instanceMaskIds);
};

} // namespace aion::gameserver::network::aion::serverpackets
