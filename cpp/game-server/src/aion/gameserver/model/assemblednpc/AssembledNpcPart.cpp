#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::assemblednpc {

AssembledNpcPart::AssembledNpcPart(std::optional<int32_t> value,
	const templates::assemblednpc::AssembledNpcTemplate::AssembledNpcPartTemplate* template_Value)
	: object(value), template_(template_Value) {
}

runtime::Ref<AssembledNpcPart> AssembledNpcPart::create(std::optional<int32_t> value,
	const templates::assemblednpc::AssembledNpcTemplate::AssembledNpcPartTemplate* template_Value) {
	return runtime::makeRef<AssembledNpcPart>(value, template_Value);
}

int32_t AssembledNpcPart::getNpcId() {
	AION_UNPORTED();
}

int32_t AssembledNpcPart::getStaticId() {
	AION_UNPORTED();
}

AssembledNpcPart::~AssembledNpcPart() = default;

} // namespace aion::gameserver::model::assemblednpc
