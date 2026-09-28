#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/transfers/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * C++: written lazily by LoginServerConnection::writeData like Java, so the ITEMS_INFORMATION branch runs its DAO calls on the IO strand (inside
 * the write TaskScope), as the Java dispatcher thread did.
 *
 * @author KID
 */
class SM_PTRANSFER_CONTROL : public LsServerPacket {
public:
	static constexpr int8_t CHARACTER_INFORMATION = 1;
	static constexpr int8_t ITEMS_INFORMATION = 5;
	static constexpr int8_t DATA_INFORMATION = 6;
	static constexpr int8_t SKILL_INFORMATION = 7;
	static constexpr int8_t RECIPE_INFORMATION = 8;
	static constexpr int8_t QUEST_INFORMATION = 9;
	static constexpr int8_t ERROR_ = 2; // Java: ERROR (<wingdi.h> defines an ERROR macro)
	static constexpr int8_t OK = 3;
	static constexpr int8_t TASK_STOP = 4;

private:
	int8_t type = 0;
	/** null for the ERROR/OK/TASK_STOP packets */
	runtime::Ref<model::gameobjects::player::Player> player;
	/** Java null writes the same bytes as "" */
	std::string result;
	int32_t taskId = 0;

public:
	SM_PTRANSFER_CONTROL(int8_t type, int32_t taskId);

	SM_PTRANSFER_CONTROL(int8_t type, services::transfers::TransferablePlayer& tp);

	SM_PTRANSFER_CONTROL(int8_t type, services::transfers::TransferablePlayer& tp, std::string_view result);

	SM_PTRANSFER_CONTROL(int8_t type, int32_t taskId, std::string_view result);

	/** C++ only: out of line, so copies for the send queue need no complete Player (LoginServer::sendPacket forwarding template) */
	SM_PTRANSFER_CONTROL(const SM_PTRANSFER_CONTROL& other);
	SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL&& other) noexcept;
	~SM_PTRANSFER_CONTROL() override;

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
