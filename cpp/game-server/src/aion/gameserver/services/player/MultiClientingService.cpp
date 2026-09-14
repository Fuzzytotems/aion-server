#include "aion/gameserver/services/player/MultiClientingService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::player {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.MultiClientingService");

MultiClientingService::AccountSession::AccountSession(int32_t value) : accountId(value) {
}

runtime::Ref<MultiClientingService::AccountSession> MultiClientingService::AccountSession::create(int32_t value) {
	return runtime::makeRef<MultiClientingService::AccountSession>(value);
}

bool MultiClientingService::AccountSession::isIgnored() {
	AION_UNPORTED();
}

void MultiClientingService::AccountSession::putIdentifiers(network::aion::AionConnection* connection) {
	AION_UNPORTED();
}

bool MultiClientingService::AccountSession::hasAny(std::string_view ip, std::string_view mac) {
	AION_UNPORTED();
}

bool MultiClientingService::AccountSession::isExpired() {
	AION_UNPORTED();
}

void MultiClientingService::AccountSession::enterWorld(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void MultiClientingService::AccountSession::leaveWorld(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool MultiClientingService::AccountSession::wasPlayingOnSameIpOrMac(model::Race race, int64_t minLastOnlineMillis,
	network::aion::AionConnection* con) {
	AION_UNPORTED();
}

MultiClientingService::AccountSession::~AccountSession() = default;

MultiClientingService::Identifiers::Identifiers(std::string_view ip, std::string_view mac)
	: ip_(ip), mac_(mac) {
}

runtime::Ref<MultiClientingService::Identifiers> MultiClientingService::Identifiers::create(std::string_view ip, std::string_view mac) {
	return runtime::makeRef<Identifiers>(ip, mac);
}

bool MultiClientingService::Identifiers::equals(const Identifiers& obj) const {
	return ip_ == obj.ip_ && mac_ == obj.mac_;
}

int32_t MultiClientingService::Identifiers::hashCode() const {
	// Java: String.hashCode/Enum identity hash of the components; ported with the first hash collection that holds the record
	AION_UNPORTED();
}

MultiClientingService::Identifiers::~Identifiers() = default;

bool MultiClientingService::tryEnterWorld(model::gameobjects::player::Player& player, network::aion::AionConnection* con) {
	AION_UNPORTED();
}

void MultiClientingService::onLeaveWorld(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

std::optional<int32_t> MultiClientingService::checkForFactionSwitchCooldownTime(model::Race race, network::aion::AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::player
