#include "aion/gameserver/services/SocialService.h"

#include "aion/gameserver/dao/BlockListDAO.h"
#include "aion/gameserver/dao/FriendListDAO.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_NOTIFY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_RESPONSE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

using network::aion::serverpackets::SM_BLOCK_LIST;
using network::aion::serverpackets::SM_BLOCK_RESPONSE;
using network::aion::serverpackets::SM_FRIEND_LIST;
using network::aion::serverpackets::SM_FRIEND_NOTIFY;
using network::aion::serverpackets::SM_FRIEND_RESPONSE;
using utils::PacketSendUtility;

// Java SocialService.java:20-28
bool SocialService::addBlockedUser(model::gameobjects::player::Player& player, model::gameobjects::player::PlayerCommonData& blockedPlayer,
	std::string_view reason) {
	if (dao::BlockListDAO::addBlockedUser(player.getObjectId(), blockedPlayer.getPlayerObjId(), reason)) {
		player.getBlockList()->add(*model::gameobjects::player::BlockedPlayer::create(blockedPlayer.getPlayerObjId(), blockedPlayer.getName(), reason));
		PacketSendUtility::sendPacket(player, SM_BLOCK_LIST());
		PacketSendUtility::sendPacket(player, SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::BLOCK_SUCCESSFUL, blockedPlayer.getName()));
		return true;
	}
	return false;
}

// Java SocialService.java:30-38
bool SocialService::deleteBlockedUser(model::gameobjects::player::Player& player, model::gameobjects::player::BlockedPlayer& target) {
	if (dao::BlockListDAO::delBlockedUser(player.getObjectId(), target.getObjId())) {
		player.getBlockList()->remove(target.getObjId());
		PacketSendUtility::sendPacket(player, SM_BLOCK_LIST());
		PacketSendUtility::sendPacket(player, SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::UNBLOCK_SUCCESSFUL, target.getName()));
		return true;
	}
	return false;
}

// Java SocialService.java:51-61
bool SocialService::setBlockedReason(model::gameobjects::player::Player& player, model::gameobjects::player::BlockedPlayer& target,
	std::string_view reason) {
	if (target.getReason() != reason) { // Java: !target.getReason().equals(reason)
		if (dao::BlockListDAO::setReason(player.getObjectId(), target.getObjId(), reason)) {
			target.setReason(reason);
			PacketSendUtility::sendPacket(player, SM_BLOCK_LIST());
			PacketSendUtility::sendPacket(player, SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::EDIT_NOTE, target.getName()));
			return true;
		}
	}
	return false;
}

// Java SocialService.java:66-75
bool SocialService::setFriendMemo(model::gameobjects::player::Player& player, model::gameobjects::player::Friend& target, std::string_view memo) {
	if (target.getFriendMemo() != memo) { // Java: !target.getFriendMemo().equals(memo)
		if (dao::FriendListDAO::setFriendMemo(player.getObjectId(), target.getObjectId(), memo)) {
			target.setFriendMemo(memo);
			PacketSendUtility::sendPacket(player, SM_FRIEND_LIST());
			return true;
		}
	}
	return false;
}

// Java SocialService.java:82-97
bool SocialService::makeFriends(model::gameobjects::player::Player& friend1, model::gameobjects::player::Player& friend2) {
	if (friend1.getFriendList().getFriend(friend2.getObjectId()) != nullptr)
		return false;
	if (dao::FriendListDAO::addFriends(friend1, friend2)) {
		friend1.getFriendList().addFriend(*model::gameobjects::player::Friend::create(*friend2.getCommonData(), ""));
		friend2.getFriendList().addFriend(*model::gameobjects::player::Friend::create(*friend1.getCommonData(), ""));

		PacketSendUtility::sendPacket(friend1, SM_FRIEND_LIST());
		PacketSendUtility::sendPacket(friend2, SM_FRIEND_LIST());

		PacketSendUtility::sendPacket(friend1, SM_FRIEND_RESPONSE::TARGET_ADDED(friend2.getName()));
		PacketSendUtility::sendPacket(friend2, SM_FRIEND_RESPONSE::TARGET_ADDED(friend1.getName()));
		return true;
	}
	return false;
}

// Java SocialService.java:108-125
bool SocialService::deleteFriend(model::gameobjects::player::Player& deleter, model::gameobjects::player::Friend& friend_) {
	int32_t friendObjId = friend_.getObjectId();
	if (dao::FriendListDAO::delFriends(deleter.getObjectId(), friendObjId)) {
		runtime::Ptr<model::gameobjects::player::Player> friendPlayer = world::World::getInstance().getPlayer(friendObjId);
		if (friendPlayer != nullptr) {
			friendPlayer->getFriendList().delFriend(deleter.getObjectId());
			PacketSendUtility::sendPacket(*friendPlayer, SM_FRIEND_LIST());
			PacketSendUtility::sendPacket(*friendPlayer, SM_FRIEND_NOTIFY(SM_FRIEND_NOTIFY::DELETED, deleter.getName()));
		}
		// Delete from deleter's friend list and send packets
		deleter.getFriendList().delFriend(friendObjId);
		PacketSendUtility::sendPacket(deleter, SM_FRIEND_LIST());
		PacketSendUtility::sendPacket(deleter, SM_FRIEND_RESPONSE::TARGET_REMOVED(friend_.getName()));
		return true;
	}
	return false;
}

} // namespace aion::gameserver::services
