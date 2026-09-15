#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/assemblednpc/fwd.h"
#include "aion/gameserver/model/templates/assemblednpc/AssembledNpcTemplate.h"

namespace aion::gameserver::model::assemblednpc {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class AssembledNpcPart : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const std::optional<int32_t> object;
	// Java field `template` (a C++ keyword: template_)
	const templates::assemblednpc::AssembledNpcTemplate::AssembledNpcPartTemplate* template_;
protected:
	AssembledNpcPart(std::optional<int32_t> object, const templates::assemblednpc::AssembledNpcTemplate::AssembledNpcPartTemplate* template_);

public:
	static runtime::Ref<AssembledNpcPart> create(std::optional<int32_t> value,
		const templates::assemblednpc::AssembledNpcTemplate::AssembledNpcPartTemplate* template_Value);

	std::optional<int32_t> getObject() const { return this->object; }

	const templates::assemblednpc::AssembledNpcTemplate::AssembledNpcPartTemplate* getAssembledNpcPartTemplate() const { return template_; }

	int32_t getNpcId();

	int32_t getStaticId();

protected:
	~AssembledNpcPart() override;
};

} // namespace aion::gameserver::model::assemblednpc
