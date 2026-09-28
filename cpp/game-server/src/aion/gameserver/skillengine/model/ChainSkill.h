#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5): a member type of ChainSkills. RefCounted (fieldmap K4), created with create().
 *
 * @author kecimis, Neon
 */
class ChainSkill : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<std::string> category{};
	runtime::Field<int32_t> useCount{0};
	runtime::Field<int64_t> lastUseTime{0};

protected:
	explicit ChainSkill(std::string_view category);
	~ChainSkill() override;

public:
	/** Java: new ChainSkill(category) */
	static runtime::Ref<ChainSkill> create(std::string_view category);

	void clear();

	std::string getCategory() const { return category.get(); }

	void setCategory(std::string_view name) { category.set(std::string(name)); }

	int32_t getUseCount() const { return useCount.get(); }

	void increaseUseCount();

	/**
	 * @return The time when this chain skill was last activated, 0 if never.
	 */
	int64_t getLastUseTime() const { return lastUseTime.get(); }
};

} // namespace aion::gameserver::skillengine::model
