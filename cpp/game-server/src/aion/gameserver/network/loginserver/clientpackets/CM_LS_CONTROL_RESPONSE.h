#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * @author Aionchs-Wylovech
 */
class CM_LS_CONTROL_RESPONSE : public LsClientPacket {
public:
	explicit CM_LS_CONTROL_RESPONSE(int32_t opCode);

private:
	int8_t type = 0;
	int8_t param = 0;
	int32_t accountId = 0;
	int32_t adminId = 0;
	bool result = false;

public:
	void readImpl() override;

	void runImpl() override;

private:
	/** @param admin may be null (the admin went offline) */
	void notifyAboutNewPermissions(runtime::Ptr<model::gameobjects::player::Player> admin, runtime::Ptr<model::gameobjects::player::Player> player,
		std::string_view targetAccount, std::string_view permissionType);

	/** @param player may be null */
	void sendMessage(runtime::Ptr<model::gameobjects::player::Player> player, std::string_view message);
};

} // namespace aion::gameserver::network::loginserver::clientpackets
