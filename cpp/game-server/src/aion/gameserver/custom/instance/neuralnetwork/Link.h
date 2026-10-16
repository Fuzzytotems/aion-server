#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/fields/Final.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * A weighted edge of the player model (D12, owner 2026-10-13).
 *
 * @author Jo
 */
class Link : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Final<PlayerModelLink*> input{}; // fieldmap.toml: non-retaining: the neurons are owned by their PlayerModel, links only point back into it; Ref would make every link pair a cycle (cycles.toml)
	runtime::Final<PlayerModelLink*> output{}; // fieldmap.toml: non-retaining: the neurons are owned by their PlayerModel, links only point back into it; Ref would make every link pair a cycle (cycles.toml)
	runtime::Field<double> weight{};
	runtime::Field<double> weightDelta{};
protected:
	Link(PlayerModelLink& input, PlayerModelLink& output);
public:
	static runtime::Ref<Link> create(PlayerModelLink& input, PlayerModelLink& output);
	PlayerModelLink& getInput() const { return *this->input.get(); }
	PlayerModelLink& getOutput() const { return *this->output.get(); }
	double getWeight() const { return this->weight.get(); }
	void setWeight(double value) { this->weight.set(value); }
	double getWeightDelta() const { return this->weightDelta.get(); }
	void setWeightDelta(double value) { this->weightDelta.set(value); }
protected:
	~Link() override;
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
