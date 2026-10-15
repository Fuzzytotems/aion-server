#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/account/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ViAl, Luzien, SVDNESS
 */
class AccountPassportsDAO {
public:
	static void loadPassport(model::account::Account& account);
	static void storePassportList(int32_t accountId, const std::vector<runtime::Ptr<model::account::Passport>>& pList);
	static void storePassport(model::account::Account& account);
private:
	static void addPassports(int32_t accountId, model::account::Passport& passport);
	static void updatePassport(int32_t accountId, model::account::Passport& passport);
	static void deletePassport(int32_t accountId, model::account::Passport& passport);
	static void insertStamps(int32_t accountId);
	static void updateStamps(model::account::Account& account);
public:
	static void resetAllLastStamps();
	static void resetAllStamps();
private:
	static std::optional<commons::database::Timestamp> normTs(std::optional<commons::database::Timestamp> ts);
};

} // namespace aion::gameserver::dao
