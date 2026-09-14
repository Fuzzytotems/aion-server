#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/services/transfers/fwd.h"

namespace aion::gameserver::services::transfers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * The byte[] fields are runtime Arrays passed and returned as such (like Player.captchaImage).
 *
 * @author xTz
 */
class PlayerTransfer : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> commonData{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> itemsData{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> data{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> recipeData{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> skillData{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> questData{};
	const int32_t taskId;
	const int32_t targetAccount;
	const std::string name;
	const std::string account;

protected:
	PlayerTransfer(int32_t taskId, int32_t targetAccount, std::string_view account, std::string_view name);

public:
	static runtime::Ref<PlayerTransfer> create(int32_t value, int32_t targetAccountValue, std::string_view accountValue, std::string_view nameValue);

	std::string getAccount() const { return this->account; }

	int32_t getTargetAccount() const { return this->targetAccount; }

	std::string getName() const { return this->name; }

	int32_t getTaskId() const { return this->taskId; }

	runtime::Ptr<runtime::Array<int8_t>> getCommonData() const { return commonData.get(); }

	runtime::Ptr<runtime::Array<int8_t>> getItemsData() const { return itemsData.get(); }

	void setItemsData(runtime::Ptr<runtime::Array<int8_t>> itemsData);

	void setCommonData(runtime::Ptr<runtime::Array<int8_t>> commonData);

	runtime::Ptr<runtime::Array<int8_t>> getSkillData() const { return skillData.get(); }

	runtime::Ptr<runtime::Array<int8_t>> getRecipeData() const { return recipeData.get(); }

	runtime::Ptr<runtime::Array<int8_t>> getQuestData() const { return questData.get(); }

	runtime::Ptr<runtime::Array<int8_t>> getData() const { return data.get(); }

	void setSkillData(runtime::Ptr<runtime::Array<int8_t>> skillData);

	void setRecipeData(runtime::Ptr<runtime::Array<int8_t>> recipeData);

	void setQuestData(runtime::Ptr<runtime::Array<int8_t>> questData);

	void setData(runtime::Ptr<runtime::Array<int8_t>> data);

	/** Java: a new little-endian ByteBuffer with all data arrays; C++: its bytes */
	std::vector<uint8_t> getDB();

protected:
	~PlayerTransfer() override;
};

} // namespace aion::gameserver::services::transfers
