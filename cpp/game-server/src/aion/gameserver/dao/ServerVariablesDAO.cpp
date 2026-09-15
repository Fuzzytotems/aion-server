#include "aion/gameserver/dao/ServerVariablesDAO.h"

#include <string>
#include <typeinfo>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ServerVariablesDAO");

namespace {

/**
 * Java: value.toString() of the boxed value. The Java callers pass Integer and Long values (GameTimeService, PeriodicSaveService); the other
 * integer types, bool and strings are converted the same way. An empty std::any is Java's null (NullPointerException).
 */
std::string toJavaString(const std::any& value) {
	if (!value.has_value())
		throw runtime::NullPointerException("Cannot invoke \"Object.toString()\" because \"value\" is null");
	const std::type_info& type = value.type();
	if (type == typeid(int32_t))
		return std::to_string(std::any_cast<int32_t>(value));
	if (type == typeid(int64_t))
		return std::to_string(std::any_cast<int64_t>(value));
	if (type == typeid(int16_t))
		return std::to_string(std::any_cast<int16_t>(value));
	if (type == typeid(int8_t))
		return std::to_string(std::any_cast<int8_t>(value));
	if (type == typeid(uint32_t))
		return std::to_string(std::any_cast<uint32_t>(value));
	if (type == typeid(uint64_t))
		return std::to_string(std::any_cast<uint64_t>(value));
	if (type == typeid(bool))
		return std::any_cast<bool>(value) ? "true" : "false";
	if (type == typeid(std::string))
		return std::any_cast<std::string>(value);
	if (type == typeid(std::string_view))
		return std::string(std::any_cast<std::string_view>(value));
	if (type == typeid(const char*))
		return std::string(std::any_cast<const char*>(value));
	throw commons::utils::IllegalArgumentException(std::string("Unsupported server variable value type ") + type.name());
}

} // namespace

std::optional<int32_t> ServerVariablesDAO::loadInt(std::string_view var) {
	std::optional<std::string> value = load(var);
	return !value ? std::nullopt : std::optional<int32_t>(commons::utils::parseInt(*value));
}

std::optional<int64_t> ServerVariablesDAO::loadLong(std::string_view var) {
	std::optional<std::string> value = load(var);
	return !value ? std::nullopt : std::optional<int64_t>(commons::utils::parseLong(*value));
}

bool ServerVariablesDAO::store(std::string_view var, const std::any& value) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("REPLACE INTO `server_variables` (`key`,`value`) VALUES (?,?)");
		ps->setString(1, var);
		ps->setString(2, toJavaString(value));
		return ps->executeUpdate() > 0;
	} catch (const SQLException& e) {
		// Java string concatenation prints null for a null value
		log.error("Error storing " + (value.has_value() ? toJavaString(value) : std::string("null")) + " for variable " + std::string(var), e);
		return false;
	}
}

bool ServerVariablesDAO::delete_(std::string_view var) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("DELETE FROM `server_variables` WHERE `key`=?");
		ps->setString(1, var);
		return ps->executeUpdate() > 0;
	} catch (const SQLException& e) {
		log.error("Error loading value for " + std::string(var), e);
		return false;
	}
}

std::optional<std::string> ServerVariablesDAO::load(std::string_view var) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("SELECT `value` FROM `server_variables` WHERE `key`=?");
		ps->setString(1, var);
		auto rs = ps->executeQuery();
		if (rs->next())
			return rs->getObject<std::string>("value");
	} catch (const SQLException& e) {
		log.error("Error loading value for " + std::string(var), e);
	}
	return std::nullopt;
}

} // namespace aion::gameserver::dao
