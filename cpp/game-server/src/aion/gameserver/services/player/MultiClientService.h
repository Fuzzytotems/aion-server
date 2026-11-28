#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayDeque.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ENTER_WORLD_CHECK.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

class MultiClientService {
private:
	class AccountSession;
	class Identifiers;
	class AccountSession : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const int32_t accountId{};
		runtime::ConcurrentHashMap<model::Race, int64_t> lastCharOnlineTimeMillis{
			AION_LOCK_CLASS(MultiClientService::AccountSession::lastCharOnlineTimeMillis#stripe)};
		runtime::ArrayDeque<runtime::Ref<MultiClientService::Identifiers>> identifiers{
			AION_LOCK_CLASS(MultiClientService::AccountSession::identifiers)}; // Java: = new ArrayDeque<>(2)
	protected:
		explicit AccountSession(int32_t accountId);
	public:
		static runtime::Ref<MultiClientService::AccountSession> create(int32_t accountId);
		bool isIgnored(); // synchronized
		void putIdentifiers(network::aion::AionConnection* connection); // synchronized
		bool hasAny(std::string_view ip, std::string_view mac); // synchronized
		bool isExpired();
		void enterWorld(model::gameobjects::player::Player& player);
		void leaveWorld(model::gameobjects::player::Player& player);
		bool wasPlayingOnSameIpOrMac(model::Race race, int64_t minLastOnlineMillis, network::aion::AionConnection* con);
	protected:
		~AccountSession() override;
	};
	class Identifiers : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const std::string ip_{};
		const std::string mac_{};
	protected:
		Identifiers(std::string_view ip, std::string_view mac); // canonical record constructor
	public:
		static runtime::Ref<MultiClientService::Identifiers> create(std::string_view ip, std::string_view mac);
		std::string ip() const { return this->ip_; }
		std::string mac() const { return this->mac_; }
		/** Java record equals: all components */
		bool equals(const Identifiers& obj) const;
		int32_t hashCode() const;
	protected:
		~Identifiers() override;
	};
	/**
	 * A computer is identified by the IP address along with the MAC address, so different computers behind the same NAT don't collide. HDD serials
	 * are not used, since the client detects and reads them unreliably.
	 */
	static std::vector<runtime::Ptr<model::gameobjects::player::Player>> findPlayersOnSameComputer(network::aion::AionConnection* con);
	static inline runtime::ConcurrentHashMap<int32_t, runtime::Ref<MultiClientService::AccountSession>> sessionsByAccountId{
		AION_LOCK_CLASS(MultiClientService::sessionsByAccountId#stripe)};
public:
	/** @return The reason why the player is not allowed to enter the world, or null (std::nullopt) if they are. */
	static std::optional<network::aion::serverpackets::SM_ENTER_WORLD_CHECK::Msg> tryEnterWorld(model::gameobjects::player::Player& player,
		network::aion::AionConnection* con);
	static void onLeaveWorld(model::gameobjects::player::Player& player);
	static std::optional<int32_t> checkForFactionSwitchCooldownTime(model::Race race, network::aion::AionConnection* con);
};

} // namespace aion::gameserver::services::player
