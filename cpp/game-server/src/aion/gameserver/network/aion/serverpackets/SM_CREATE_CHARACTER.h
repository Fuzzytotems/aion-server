#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is response for CM_CREATE_CHARACTER
 * <p>
 * C++: writeImpl reads the connection through AbstractPlayerInfoPacket::writePlayerInfo (getCharBanInfo), so it is serialized per recipient
 * like SM_CHARACTER_LIST (runtime-architecture.md §8.3).
 *
 * @author Nemesiss, AEJTester, Neon
 */
class SM_CREATE_CHARACTER : public AbstractPlayerInfoPacket {
public:
	static constexpr int32_t RESPONSE_OK = 0x00;
	static constexpr int32_t FAILED_TO_CREATE_THE_CHARACTER = 1;
	static constexpr int32_t RESPONSE_DB_ERROR = 2;
	static constexpr int32_t RESPONSE_SERVER_LIMIT_EXCEEDED = 4;
	static constexpr int32_t RESPONSE_INVALID_NAME = 5;
	static constexpr int32_t RESPONSE_FORBIDDEN_CHAR_NAME = 9;
	static constexpr int32_t RESPONSE_NAME_ALREADY_USED = 10;
	static constexpr int32_t RESPONSE_NAME_RESERVED = 11;
	static constexpr int32_t RESPONSE_OTHER_RACE = 12;
	static constexpr int32_t RESPONSE_FORBIDDEN_CLASS = 20;
	static constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;

private:
	int32_t responseCode{};
	runtime::Ref<model::account::PlayerAccountData> playerAccData{};

public:
	/** Constructs new <tt>SM_CREATE_CHARACTER</tt> packet */
	SM_CREATE_CHARACTER(runtime::Ptr<model::account::PlayerAccountData> accPlData, int32_t responseCode);
	~SM_CREATE_CHARACTER() override;
	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
