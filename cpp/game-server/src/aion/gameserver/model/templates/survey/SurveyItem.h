#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/survey/fwd.h"

namespace aion::gameserver::model::templates::survey {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author KID
 */
class SurveyItem : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	runtime::Field<int32_t> ownerId{};
	runtime::Field<int32_t> uniqueId{};
	runtime::Field<int32_t> itemId{};
	runtime::Field<int64_t> count{};
	runtime::Field<std::string> html{};
	runtime::Field<std::string> radio{};

protected:
	/** Java: the implicit default constructor */
	SurveyItem();

public:
	static runtime::Ref<SurveyItem> create();

protected:
	~SurveyItem() override;
};

} // namespace aion::gameserver::model::templates::survey
