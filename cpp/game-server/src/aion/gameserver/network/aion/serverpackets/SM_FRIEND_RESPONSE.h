#pragma once

#include <cstdint>
#include <memory>
#include <memory>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Replies to a request to add or delete a friend
 *
 * @author Ben, Neon
 */
class SM_FRIEND_RESPONSE : public AionServerPacket {
public:
	/**
	 * Java: new SM_FRIEND_RESPONSE(0x1), a packet sent as it is (`sendPacket(player, *SM_FRIEND_RESPONSE::TARGET_OFFLINE)`); defined in the .cpp
	 */
	static const std::shared_ptr<SM_FRIEND_RESPONSE> TARGET_OFFLINE;
	static const std::shared_ptr<SM_FRIEND_RESPONSE> TARGET_ALREADY_FRIEND;
	static const std::shared_ptr<SM_FRIEND_RESPONSE> TARGET_NOT_FOUND;
	static const std::shared_ptr<SM_FRIEND_RESPONSE> LIST_FULL;
	static const std::shared_ptr<SM_FRIEND_RESPONSE> TARGET_BLOCKED_YOU;
	static const std::shared_ptr<SM_FRIEND_RESPONSE> TARGET_DEAD;
	static const std::shared_ptr<SM_FRIEND_RESPONSE> TOO_MANY_REQUESTS;
	static const std::shared_ptr<SM_FRIEND_RESPONSE> CLOSE_SEND_REQUEST_WINDOW;

private:
	std::string playerName{};
	int32_t code{};

public:
	/** You have successfully added %s to your friend list. */
	static SM_FRIEND_RESPONSE TARGET_ADDED(std::string_view targetName);
	/** %s denied your request to add him. */
	static SM_FRIEND_RESPONSE TARGET_DENIED(std::string_view targetName);
	/** You have removed %s from your friend list. */
	static SM_FRIEND_RESPONSE TARGET_REMOVED(std::string_view targetName);
	/** The friend list of %s is full. */
	static SM_FRIEND_RESPONSE TARGET_LIST_FULL(std::string_view targetName);
	/** %s is currently not online. The friend request has been sent though. */
	static SM_FRIEND_RESPONSE TARGET_OFFLINE_SENT_REQUEST(std::string_view targetName);
	/** A friend request to %s exists already. */
	static SM_FRIEND_RESPONSE TARGET_REQUESTED_ALREADY(std::string_view targetName);
	/** The friend list of %s is full. Accepting requests is not possible anymore. */
	static SM_FRIEND_RESPONSE REQUESTER_LIST_FULL_CANT_ACCEPT(std::string_view targetName);
	/** You have denied the friend request from %s. */
	static SM_FRIEND_RESPONSE REQUEST_DENIED(std::string_view requesterName);
	/** You have already received a request from %s. */
	static SM_FRIEND_RESPONSE REQUEST_ALREADY_RECEIVED(std::string_view targetName);
	explicit SM_FRIEND_RESPONSE(int32_t messageType);
	SM_FRIEND_RESPONSE(std::string_view playerName, int32_t messageType);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
