#include "aion/gameserver/services/player/MultiClientService.h"

#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/world/World.h"
#include "aion/commons/utils/WindowsMacroGuard.h"

namespace aion::gameserver::services::player {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.MultiClientService");

MultiClientService::AccountSession::AccountSession(int32_t value) : accountId(value) {
}

runtime::Ref<MultiClientService::AccountSession> MultiClientService::AccountSession::create(int32_t value) {
	return runtime::makeRef<MultiClientService::AccountSession>(value);
}

bool MultiClientService::AccountSession::isIgnored() {
	SYNCHRONIZED(*this) {
		return !identifiers.isEmpty() && configs::main::SecurityConfig::MULTI_CLIENT_IGNORED_MAC_ADDRESSES.get()->contains(identifiers.getFirst()->mac());
	}
}

void MultiClientService::AccountSession::putIdentifiers(network::aion::AionConnection* connection) {
	SYNCHRONIZED(*this) {
		runtime::Ref<Identifiers> ids = Identifiers::create(connection->getIP(), connection->getMacAddress());
		if (!identifiers.contains(ids)) {
			identifiers.addFirst(ids);
			while (identifiers.size() > 3)
				identifiers.removeLast();
		}
	}
}

bool MultiClientService::AccountSession::hasAny(std::string_view ip, std::string_view mac) {
	SYNCHRONIZED(*this) {
		for (const runtime::Ref<Identifiers>& ids : identifiers.snapshot()) {
			if (ids->ip() == ip || ids->mac() == mac)
				return true;
		}
		return false;
	}
}

bool MultiClientService::AccountSession::isExpired() {
	int64_t minLastOnline = commons::utils::currentTimeMillis()
		- std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::minutes(configs::main::SecurityConfig::MULTI_CLIENT_FACTION_SWITCH_COOLDOWN_MINUTES.load())).count();
	for (int64_t t : lastCharOnlineTimeMillis.values().toVector()) {
		if (t > minLastOnline)
			return false;
	}
	return true;
}

void MultiClientService::AccountSession::enterWorld(model::gameobjects::player::Player& player) {
	lastCharOnlineTimeMillis.put(player.getRace(), std::numeric_limits<int64_t>::max());
}

void MultiClientService::AccountSession::leaveWorld(model::gameobjects::player::Player& player) {
	lastCharOnlineTimeMillis.put(player.getRace(), commons::utils::currentTimeMillis());
}

bool MultiClientService::AccountSession::wasPlayingOnSameIpOrMac(model::Race race, int64_t minLastOnlineMillis,
	network::aion::AionConnection* con) {
	std::optional<int64_t> lastOnlineMillis = lastCharOnlineTimeMillis.get(race);
	return lastOnlineMillis && *lastOnlineMillis > minLastOnlineMillis && hasAny(con->getIP(), con->getMacAddress());
}

MultiClientService::AccountSession::~AccountSession() = default;

MultiClientService::Identifiers::Identifiers(std::string_view ip, std::string_view mac)
	: ip_(ip), mac_(mac) {
}

runtime::Ref<MultiClientService::Identifiers> MultiClientService::Identifiers::create(std::string_view ip, std::string_view mac) {
	return runtime::makeRef<Identifiers>(ip, mac);
}

bool MultiClientService::Identifiers::equals(const Identifiers& obj) const {
	return ip_ == obj.ip_ && mac_ == obj.mac_;
}

int32_t MultiClientService::Identifiers::hashCode() const {
	// Java record hashCode (java.lang.runtime.ObjectMethods): 31 * h + hash(component); String.hashCode over the UTF-16 code units
	const auto stringHash = [](const std::string& value) {
		uint32_t hash = 0;
		for (char16_t c : commons::utils::StringUtils::toUtf16(value))
			hash = 31 * hash + c;
		return hash;
	};
	uint32_t h = stringHash(ip_);
	h = 31 * h + stringHash(mac_);
	return static_cast<int32_t>(h);
}

MultiClientService::Identifiers::~Identifiers() = default;

std::optional<network::aion::serverpackets::SM_ENTER_WORLD_CHECK::Msg> MultiClientService::tryEnterWorld(model::gameobjects::player::Player& player,
	network::aion::AionConnection* con) {
	using configs::main::SecurityConfig;
	using Msg = network::aion::serverpackets::SM_ENTER_WORLD_CHECK::Msg;
	int32_t maxClients = SecurityConfig::MULTI_CLIENT_MAX_CLIENTS_PER_COMPUTER.load();
	if (maxClients > 0 && !SecurityConfig::MULTI_CLIENT_IGNORED_MAC_ADDRESSES.get()->contains(con->getMacAddress())) {
		std::vector<runtime::Ptr<model::gameobjects::player::Player>> playersOnSameComputer = findPlayersOnSameComputer(con);
		if (std::cmp_greater_equal(playersOnSameComputer.size(), maxClients)) {
			std::string players = "[";
			for (size_t i = 0; i < playersOnSameComputer.size(); i++)
				players += (i > 0 ? ", " : "") + playersOnSameComputer[i]->toString();
			players += "]";
			log.info("Blocked {} from logging on (multi-client limit of {} reached by {})", player.toString(), maxClients, players);
			return Msg::CONNECTION_ERROR;
		}
	}
	if (!SecurityConfig::MULTI_CLIENT_ALLOW_BOTH_FACTIONS.load()) {
		sessionsByAccountId.values().removeIf([](const auto& session) { return session->isExpired(); });
		SYNCHRONIZED(sessionsByAccountId) {
			std::optional<int32_t> matchedAccountId = checkForFactionSwitchCooldownTime(player.getRace(), con);
			if (matchedAccountId) {
				log.info("Blocked {} from logging on (faction switch cooldown from account ID {})", player.toString(), *matchedAccountId);
				return Msg::BOTH_FACTIONS;
			}
			runtime::Ptr<AccountSession> accountSession =
				sessionsByAccountId.computeIfAbsent(player.getAccount()->getId(), [](int32_t accountId) { return AccountSession::create(accountId); });
			accountSession->putIdentifiers(con);
			accountSession->enterWorld(player);
		}
	}
	return std::nullopt;
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> MultiClientService::findPlayersOnSameComputer(network::aion::AionConnection* con) {
	std::string ip = con->getIP();
	std::string mac = con->getMacAddress();
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> players;
	for (const runtime::Ptr<model::gameobjects::player::Player>& onlinePlayer : world::World::getInstance().getAllPlayers()) {
		std::shared_ptr<network::aion::AionConnection> onlineCon = onlinePlayer->getClientConnection();
		if (onlineCon && ip == onlineCon->getIP() && mac == onlineCon->getMacAddress())
			players.push_back(onlinePlayer);
	}
	return players;
}

void MultiClientService::onLeaveWorld(model::gameobjects::player::Player& player) {
	runtime::Ptr<AccountSession> session = sessionsByAccountId.get(player.getAccount()->getId());
	if (session)
		session->leaveWorld(player);
}

std::optional<int32_t> MultiClientService::checkForFactionSwitchCooldownTime(model::Race race, network::aion::AionConnection* con) {
	using configs::main::SecurityConfig;
	if (SecurityConfig::MULTI_CLIENT_IGNORED_MAC_ADDRESSES.get()->contains(con->getMacAddress()))
		return std::nullopt;
	model::Race oppositeRace = race == model::Race::ELYOS ? model::Race::ASMODIANS : model::Race::ELYOS;
	int64_t minLastOnlineMillis = commons::utils::currentTimeMillis()
		- std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::minutes(SecurityConfig::MULTI_CLIENT_FACTION_SWITCH_COOLDOWN_MINUTES.load())).count();
	for (const auto& session : sessionsByAccountId.values().toVector()) {
		if (!session->isIgnored() && session->wasPlayingOnSameIpOrMac(oppositeRace, minLastOnlineMillis, con))
			return session->accountId; // Java: findAny
	}
	return std::nullopt;
}

} // namespace aion::gameserver::services::player
