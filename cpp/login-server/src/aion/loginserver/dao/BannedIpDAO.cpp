#include "aion/loginserver/dao/BannedIpDAO.h"

#include <string>

#include "aion/commons/database/DB.h"

namespace aion::loginserver::dao::BannedIpDAO {

using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::Timestamp;
using model::BannedIP;

std::optional<BannedIP> insert(std::string_view mask) {
	return insert(mask, std::nullopt);
}

std::optional<BannedIP> insert(std::string_view mask, std::optional<Timestamp> expireTime) {
	BannedIP result;
	result.setMask(std::string(mask));
	result.setTimeEnd(expireTime);
	if (insert(result))
		return result;
	return std::nullopt;
}

bool insert(const BannedIP& bannedIP) {
	return DB::insertUpdate("INSERT INTO banned_ip(mask, time_end) VALUES (?, ?)", [&](PreparedStatement& ps) {
		ps.setString(1, bannedIP.getMask());
		ps.setTimestamp(2, bannedIP.getTimeEnd());
		ps.execute();
	});
}

bool update(const BannedIP& bannedIP) {
	return DB::insertUpdate("UPDATE banned_ip SET mask = ?, time_end = ? WHERE id = ?", [&](PreparedStatement& ps) {
		ps.setString(1, bannedIP.getMask());
		ps.setTimestamp(2, bannedIP.getTimeEnd());
		ps.setInt(3, bannedIP.getId().value());
		ps.execute();
	});
}

bool remove(std::string_view mask) {
	return DB::insertUpdate("DELETE FROM banned_ip WHERE mask = ?", [&](PreparedStatement& ps) {
		ps.setString(1, mask);
		ps.execute();
	});
}

bool remove(const BannedIP& bannedIP) {
	return DB::insertUpdate("DELETE FROM banned_ip WHERE mask = ?", [&](PreparedStatement& ps) {
		// Changed from id to mask because we don't get id of last inserted ban
		ps.setString(1, bannedIP.getMask());
		ps.execute();
	});
}

std::unordered_set<BannedIP> getAllBans() {
	std::unordered_set<BannedIP> result;
	DB::select("SELECT * FROM banned_ip", [&](ResultSet& rs) {
		while (rs.next()) {
			BannedIP ip;
			ip.setId(rs.getInt("id"));
			ip.setMask(rs.getString("mask"));
			ip.setTimeEnd(rs.getTimestamp("time_end"));
			result.insert(std::move(ip));
		}
	});
	return result;
}

void cleanExpiredBans() {
	DB::insertUpdate("DELETE FROM banned_ip WHERE time_end < current_timestamp AND time_end IS NOT NULL");
}

} // namespace aion::loginserver::dao::BannedIpDAO
