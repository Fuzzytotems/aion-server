#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "aion/commons/database/SqlTypes.h"
#include "aion/loginserver/model/AccountTime.h"

namespace aion::loginserver::model {

/**
 * This class represents Account model
 * <p>
 * <b>Sharing and threads.</b> In the Java server one Account object is referenced by the LoginConnection, the GameServerInfo the account plays
 * on, the reconnect state (ReconnectingAccount) and player transfer requests, and it is read and modified by the Aion client packet threads,
 * the game server packet threads and the player transfer thread (e.g. lastServer in AccountController.checkAuth, activated in
 * PlayerTransferService, the AccountTime in AccountTimeController and CM_BAN). Therefore:
 * <ul>
 * <li>Accounts are used via std::shared_ptr&lt;Account&gt; (AccountDAO::getAccount returns one) and are neither copyable nor movable.</li>
 * <li><b>All fields</b> are guarded by one internal mutex, so every getter and setter may be called from any thread. Getters return copies
 * (strings, the AccountTime), never references into the object.</li>
 * <li>Individual accessors are atomic, sequences of them are not (same as with Java's unsynchronized fields). A read-modify-write of the
 * AccountTime that must not lose concurrent updates uses modifyAccountTime, and one that is stored afterwards modifyAndStoreAccountTime.</li>
 * </ul>
 * Java null values: the id and creation date are std::nullopt until the account is stored, lastIp/ipForce/allowedHddSerial are std::nullopt for
 * SQL NULL, the AccountTime is std::nullopt until it is set (AccountController.loadAccount, AccountDAO::insertAccount).
 * <p>
 * Java: com.aionemu.loginserver.model.Account
 *
 * @author SoulKeeper
 */
class Account {
public:
	Account() = default;
	Account(const Account&) = delete;
	Account& operator=(const Account&) = delete;

	/** @return account id, std::nullopt if not stored in DB */
	std::optional<int32_t> getId() const;
	void setId(std::optional<int32_t> id);

	/** @return account name */
	std::string getName() const;
	void setName(std::string name);

	/** @return password hash (Base64 encoded SHA-1, see AccountUtils::encodePassword; empty with external authentication) */
	std::string getPasswordHash() const;
	void setPasswordHash(std::string passwordHash);

	/** @return the account creation date, std::nullopt if not stored in DB yet */
	std::optional<commons::database::Timestamp> getCreationDate() const;
	void setCreationDate(std::optional<commons::database::Timestamp> creationDate);

	/** @return access level of account 0 = regular user, > 0 = GM */
	int8_t getAccessLevel() const;
	void setAccessLevel(int8_t accessLevel);

	/** @return membership of this account (regular, premium etc) */
	int8_t getMembership() const;
	void setMembership(int8_t membership);

	/** @return activation status of account (1 = activated, the account can log in) */
	int8_t getActivated() const;
	void setActivated(int8_t activated);

	/** @return last server that player visited, -1 if none */
	int8_t getLastServer() const;
	void setLastServer(int8_t lastServer);

	/** @return last ip that player played from, std::nullopt if none */
	std::optional<std::string> getLastIp() const;
	void setLastIp(std::optional<std::string> lastIp);

	/** @return last mac that player played from, "xx-xx-xx-xx-xx-xx" if none */
	std::string getLastMac() const;
	void setLastMac(std::string lastMac);

	/** @return IP (or mask) that player is forced to use with his account, std::nullopt if unrestricted */
	std::optional<std::string> getIpForce() const;
	void setIpForce(std::optional<std::string> ipForce);

	/** @return The HDD serial that this account is allowed to connect with (std::nullopt if unset; must be checked on game server side) */
	std::optional<std::string> getAllowedHddSerial() const;
	void setAllowedHddSerial(std::optional<std::string> allowedHddSerial);

	/** @return a copy of the AccountTime data, std::nullopt if not set */
	std::optional<AccountTime> getAccountTime() const;
	void setAccountTime(std::optional<AccountTime> accountTime);

	/**
	 * C++ addition: modifies the AccountTime in place while holding the account's lock (Java code mutates the shared AccountTime object returned
	 * by getAccountTime(), e.g. <code>account.getAccountTime().setPenaltyEnd(time)</code>). The modifier must not call methods of this account.
	 * If it throws, the changes it made so far remain.
	 *
	 * @return a copy of the modified AccountTime
	 * @throws commons::utils::IllegalStateException if the AccountTime is not set (Java: NullPointerException)
	 */
	template <std::invocable<AccountTime&> Modifier>
	AccountTime modifyAccountTime(Modifier&& modifier) {
		std::lock_guard lock(mutex);
		if (!accountTime)
			throwAccountTimeNotSet();
		std::forward<Modifier>(modifier)(*accountTime);
		return *accountTime;
	}

	/**
	 * C++ addition: modifyAccountTime followed by store(modified copy), serialized per account by a separate store lock that is held across both
	 * steps, so the copies of concurrent callers are stored in the order they were made and the AccountTime stored last contains every
	 * modification. Java's writers all mutate the one shared AccountTime object, whose fields the DAO reads when it binds them; storing a copy
	 * without this lock could overwrite a newer AccountTime (e.g. a CM_BAN penalty) with an older copy.
	 * <p>
	 * Lock order: the store lock is taken before the account's field lock and is held while store runs (usually database I/O), so callers must not
	 * hold other login server locks, and store must not call modifyAndStoreAccountTime on the same account.
	 *
	 * @return the result of store
	 * @throws commons::utils::IllegalStateException if the AccountTime is not set (store is not called then)
	 */
	template <std::invocable<AccountTime&> Modifier, std::invocable<const AccountTime&> Store>
	decltype(auto) modifyAndStoreAccountTime(Modifier&& modifier, Store&& store) {
		std::lock_guard storeLock(accountTimeStoreMutex);
		const AccountTime modified = modifyAccountTime(std::forward<Modifier>(modifier));
		return std::invoke(std::forward<Store>(store), modified);
	}

	/** Java: equals - true if name and password hash are equal */
	bool operator==(const Account& other) const;

	/** Java: hashCode - based on name and password hash (Java String hash codes) */
	int32_t hashCode() const;

	/** Java: Object.toString() - "com.aionemu.loginserver.model.Account@" followed by the hash code in hex (used in log messages) */
	std::string toString() const;

private:
	int32_t hashCodeLocked() const;
	[[noreturn]] void throwAccountTimeNotSet() const;

	/** serializes modifyAndStoreAccountTime; taken before mutex */
	std::mutex accountTimeStoreMutex;
	mutable std::mutex mutex;

	/** Id of account, object if assigned, null if not */
	std::optional<int32_t> id;
	/** Account name */
	std::string name;
	/** Password hash */
	std::string passwordHash;
	/** Time of account creation */
	std::optional<commons::database::Timestamp> creationDate;
	/** Access level of account 0 = regular user, > 0 = GM */
	int8_t accessLevel = 0;
	/** Membership of this account (regular, premium etc) */
	int8_t membership = 0;
	/** Account activated */
	int8_t activated = 0;
	/** last server visited by user -1 if none */
	int8_t lastServer = 0;
	/** Last ip of user -1 if none */
	std::optional<std::string> lastIp;
	/** Last mac of user xx-xx-xx-xx-xx-xx if none */
	std::string lastMac = "xx-xx-xx-xx-xx-xx";
	/** The only ip that is allowed to this account */
	std::optional<std::string> ipForce;
	/** The only HDD serial that is allowed for this account (must be checked on game server side) */
	std::optional<std::string> allowedHddSerial;
	/** AccountTime data */
	std::optional<AccountTime> accountTime;
};

} // namespace aion::loginserver::model
