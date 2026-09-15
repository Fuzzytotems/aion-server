#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"


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
	return template_->getNpcId();
}

int32_t AssembledNpcPart::getStaticId() {
	return template_->getStaticId();
}

AssembledNpcPart::~AssembledNpcPart() = default;

} // namespace aion::gameserver::model::assemblednpc
