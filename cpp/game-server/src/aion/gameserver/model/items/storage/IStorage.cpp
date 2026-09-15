#include "aion/gameserver/model/items/storage/IStorage.h"

#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::items::storage {

network::aion::serverpackets::SM_SYSTEM_MESSAGE IStorage::getStorageIsFullMessage() {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	switch (getStorageType()) {
		case StorageType::CUBE:
			return SM_SYSTEM_MESSAGE::STR_WAREHOUSE_FULL_INVENTORY();
		case StorageType::REGULAR_WAREHOUSE:
		case StorageType::ACCOUNT_WAREHOUSE:
		case StorageType::LEGION_WAREHOUSE:
			return SM_SYSTEM_MESSAGE::STR_WAREHOUSE_DEPOSIT_FULL_BASKET();
		case StorageType::PET_BAG_6:
		case StorageType::PET_BAG_12:
		case StorageType::PET_BAG_18:
		case StorageType::PET_BAG_24:
		case StorageType::CASH_PET_BAG_12:
		case StorageType::CASH_PET_BAG_18:
		case StorageType::CASH_PET_BAG_30:
		case StorageType::CASH_PET_BAG_24:
		case StorageType::PET_BAG_30:
		case StorageType::CASH_PET_BAG_26:
		case StorageType::CASH_PET_BAG_32:
		case StorageType::CASH_PET_BAG_34:
			return SM_SYSTEM_MESSAGE::STR_WAREHOUSE_TOO_MANY_ITEMS_TOYPET_WAREHOUSE();
		case StorageType::HOUSE_STORAGE_01:
		case StorageType::HOUSE_STORAGE_02:
		case StorageType::HOUSE_STORAGE_03:
		case StorageType::HOUSE_STORAGE_04:
		case StorageType::HOUSE_STORAGE_05:
		case StorageType::HOUSE_STORAGE_06:
		case StorageType::HOUSE_STORAGE_07:
		case StorageType::HOUSE_STORAGE_08:
		case StorageType::HOUSE_STORAGE_09:
		case StorageType::HOUSE_STORAGE_10:
		case StorageType::HOUSE_STORAGE_11:
		case StorageType::HOUSE_STORAGE_12:
		case StorageType::HOUSE_STORAGE_13:
		case StorageType::HOUSE_STORAGE_14:
		case StorageType::HOUSE_STORAGE_15:
		case StorageType::HOUSE_STORAGE_16:
		case StorageType::HOUSE_STORAGE_17:
		case StorageType::HOUSE_STORAGE_18:
		case StorageType::HOUSE_STORAGE_19:
		case StorageType::HOUSE_STORAGE_20:
			return SM_SYSTEM_MESSAGE::STR_HOUSING_WAREHOUSE_TOO_MANY_ITEMS_WAREHOUSE();
		case StorageType::BROKER:
			return SM_SYSTEM_MESSAGE::STR_VENDOR_FULL_ITEM();
		case StorageType::MAILBOX:
			return SM_SYSTEM_MESSAGE::STR_MAIL_SEND_FULL_BASKET();
	}
	// Java: the exhaustive switch expression throws MatchException for a value outside the enum
	throw runtime::IllegalStateException("unknown StorageType");
}

} // namespace aion::gameserver::model::items::storage
