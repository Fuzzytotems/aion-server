#include "aion/loginserver/model/Account.h"

#include <fmt/format.h>

#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/model/detail/JavaStringHash.h"

namespace aion::loginserver::model {

using commons::database::Timestamp;

std::optional<int32_t> Account::getId() const {
	std::lock_guard lock(mutex);
	return id;
}

void Account::setId(std::optional<int32_t> value) {
	std::lock_guard lock(mutex);
	id = value;
}

std::string Account::getName() const {
	std::lock_guard lock(mutex);
	return name;
}

void Account::setName(std::string value) {
	std::lock_guard lock(mutex);
	name = std::move(value);
}

std::string Account::getPasswordHash() const {
	std::lock_guard lock(mutex);
	return passwordHash;
}

void Account::setPasswordHash(std::string value) {
	std::lock_guard lock(mutex);
	passwordHash = std::move(value);
}

std::optional<Timestamp> Account::getCreationDate() const {
	std::lock_guard lock(mutex);
	return creationDate;
}

void Account::setCreationDate(std::optional<Timestamp> value) {
	std::lock_guard lock(mutex);
	creationDate = value;
}

int8_t Account::getAccessLevel() const {
	std::lock_guard lock(mutex);
	return accessLevel;
}

void Account::setAccessLevel(int8_t value) {
	std::lock_guard lock(mutex);
	accessLevel = value;
}

int8_t Account::getMembership() const {
	std::lock_guard lock(mutex);
	return membership;
}

void Account::setMembership(int8_t value) {
	std::lock_guard lock(mutex);
	membership = value;
}

int8_t Account::getActivated() const {
	std::lock_guard lock(mutex);
	return activated;
}

void Account::setActivated(int8_t value) {
	std::lock_guard lock(mutex);
	activated = value;
}

int8_t Account::getLastServer() const {
	std::lock_guard lock(mutex);
	return lastServer;
}

void Account::setLastServer(int8_t value) {
	std::lock_guard lock(mutex);
	lastServer = value;
}

std::optional<std::string> Account::getLastIp() const {
	std::lock_guard lock(mutex);
	return lastIp;
}

void Account::setLastIp(std::optional<std::string> value) {
	std::lock_guard lock(mutex);
	lastIp = std::move(value);
}

std::string Account::getLastMac() const {
	std::lock_guard lock(mutex);
	return lastMac;
}

void Account::setLastMac(std::string value) {
	std::lock_guard lock(mutex);
	lastMac = std::move(value);
}

std::optional<std::string> Account::getIpForce() const {
	std::lock_guard lock(mutex);
	return ipForce;
}

void Account::setIpForce(std::optional<std::string> value) {
	std::lock_guard lock(mutex);
	ipForce = std::move(value);
}

std::optional<std::string> Account::getAllowedHddSerial() const {
	std::lock_guard lock(mutex);
	return allowedHddSerial;
}

void Account::setAllowedHddSerial(std::optional<std::string> value) {
	std::lock_guard lock(mutex);
	allowedHddSerial = std::move(value);
}

std::optional<AccountTime> Account::getAccountTime() const {
	std::lock_guard lock(mutex);
	return accountTime;
}

void Account::setAccountTime(std::optional<AccountTime> value) {
	std::lock_guard lock(mutex);
	accountTime = std::move(value);
}

bool Account::operator==(const Account& other) const {
	if (this == &other)
		return true;
	std::scoped_lock lock(mutex, other.mutex);
	return name == other.name && passwordHash == other.passwordHash;
}

int32_t Account::hashCode() const {
	std::lock_guard lock(mutex);
	return hashCodeLocked();
}

int32_t Account::hashCodeLocked() const {
	uint32_t result = static_cast<uint32_t>(detail::javaStringHashCode(name));
	result = 31u * result + static_cast<uint32_t>(detail::javaStringHashCode(passwordHash));
	return static_cast<int32_t>(result);
}

std::string Account::toString() const {
	return fmt::format("com.aionemu.loginserver.model.Account@{:x}", static_cast<uint32_t>(hashCode()));
}

void Account::throwAccountTimeNotSet() const {
	// called with the lock held, so toString() must not be used here
	throw commons::utils::IllegalStateException(
		fmt::format("Account time of account com.aionemu.loginserver.model.Account@{:x} is not set", static_cast<uint32_t>(hashCodeLocked())));
}

} // namespace aion::loginserver::model
