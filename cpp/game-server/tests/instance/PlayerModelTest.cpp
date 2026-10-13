// D12-CP1 (owner, 2026-10-13: D12 = port): the custom instance's player model - Sigmoid, DataSet, Link, PlayerModelLink, PlayerModel,
// PlayerModelEntry, PlayerModelController (Java custom/instance/neuralnetwork). The network's values are pinned against the Java formulas,
// recomputed here over plain doubles (no Java run): the forward pass, one back-propagation step with momentum, and the second step's momentum
// term.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/custom/instance/neuralnetwork/DataSet.h"
#include "aion/gameserver/custom/instance/neuralnetwork/Link.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelController.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelEntry.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelLink.h"
#include "aion/gameserver/custom/instance/neuralnetwork/Sigmoid.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using namespace custom::instance::neuralnetwork;
using model::gameobjects::Persistable;

/** Java Sigmoid.output, recomputed */
double sig(double x) {
	return x < -45.0 ? 0.0 : x > 45.0 ? 1.0 : 1.0 / (1.0 + std::exp(static_cast<double>(static_cast<float>(-x))));
}

std::vector<runtime::Ptr<PlayerModelLink>> neurons(const runtime::ArrayList<runtime::Ref<PlayerModelLink>>& list) {
	std::vector<runtime::Ptr<PlayerModelLink>> result;
	for (runtime::Ptr<PlayerModelLink> n : list)
		result.push_back(n);
	return result;
}

std::vector<runtime::Ptr<Link>> links(const runtime::ArrayList<runtime::Ref<Link>>& list) {
	std::vector<runtime::Ptr<Link>> result;
	for (runtime::Ptr<Link> l : list)
		result.push_back(l);
	return result;
}

/** Sigmoid.java:8-14: the clamps at ±45, Math.exp of the float-rounded argument, and the derivative of an output */
TEST(PlayerModelTest, Sigmoid) {
	EXPECT_EQ(Sigmoid::output(0), 0.5);
	EXPECT_EQ(Sigmoid::output(-45.5), 0.0);
	EXPECT_EQ(Sigmoid::output(45.5), 1.0);
	EXPECT_EQ(Sigmoid::output(45.0), 1.0 / (1.0 + std::exp(static_cast<double>(static_cast<float>(-45.0)))));
	EXPECT_EQ(Sigmoid::output(0.1), 1.0 / (1.0 + std::exp(static_cast<double>(-0.1f))));
	EXPECT_NE(Sigmoid::output(0.1), 1.0 / (1.0 + std::exp(-0.1))) << "Java rounds the argument to float";
	EXPECT_EQ(Sigmoid::derivative(0.25), 0.1875);
}

/** PlayerModelController.java:56-68: the first index of the largest value above 0; 0 for none */
TEST(PlayerModelTest, GetMaxIndex) {
	EXPECT_EQ(PlayerModelController::getMaxIndex({}), 0);
	EXPECT_EQ(PlayerModelController::getMaxIndex({0.1, 0.7, 0.7, 0.2}), 1);
	EXPECT_EQ(PlayerModelController::getMaxIndex({0.1, 0.2, 0.9}), 2);
	EXPECT_EQ(PlayerModelController::getMaxIndex({-0.5, -0.1}), 0);
	EXPECT_EQ(PlayerModelController::getMaxIndex({0.0, 0.0}), 0);
}

/** PlayerModel.java:28-56: the layers and their links; innerSize below 1 is one layer; the defaults .4 and .9; the random weights in [-1, 1) */
TEST(PlayerModelTest, TheModelsLayers) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	commons::utils::Rnd::seedCurrentThreadForTests(77);
	runtime::Ref<PlayerModel> model = PlayerModel::create(3, 4, 2, 0, std::nullopt, std::nullopt);
	EXPECT_EQ(model->learnRate.get(), .4);
	EXPECT_EQ(model->momentum.get(), .9);
	EXPECT_FALSE(model->isReady.get());
	ASSERT_EQ(model->input.get()->size(), 3);
	ASSERT_EQ(model->inner.get()->size(), 1);
	ASSERT_EQ(model->inner.get()->get(0)->size(), 4);
	ASSERT_EQ(model->output.get()->size(), 2);
	for (runtime::Ptr<PlayerModelLink> in : neurons(*model->input.get())) {
		EXPECT_EQ(in->inputs.get()->size(), 0);
		EXPECT_EQ(in->outputs.get()->size(), 4);
	}
	std::vector<runtime::Ptr<PlayerModelLink>> hidden = neurons(*model->inner.get()->get(0));
	for (runtime::Ptr<PlayerModelLink> h : hidden) {
		ASSERT_EQ(h->inputs.get()->size(), 3);
		EXPECT_EQ(h->outputs.get()->size(), 2);
		for (runtime::Ptr<Link> l : links(*h->inputs.get())) {
			EXPECT_EQ(&l->getOutput(), h.get());
			EXPECT_GE(l->getWeight(), -1.0);
			EXPECT_LT(l->getWeight(), 1.0);
		}
		EXPECT_GE(h->bias.get(), -1.0);
		EXPECT_LT(h->bias.get(), 1.0);
	}
	std::vector<runtime::Ptr<Link>> outIn = links(*model->output.get()->get(1)->inputs.get());
	ASSERT_EQ(outIn.size(), 4u);
	for (size_t i = 0; i < outIn.size(); i++)
		EXPECT_EQ(&outIn[i]->getInput(), hidden[i].get()) << "an output neuron links the last hidden layer in order";

	runtime::Ref<PlayerModel> deeper = PlayerModel::create(2, 3, 1, 2, 0.1, 0.5);
	EXPECT_EQ(deeper->learnRate.get(), 0.1);
	EXPECT_EQ(deeper->momentum.get(), 0.5);
	ASSERT_EQ(deeper->inner.get()->size(), 2);
	EXPECT_EQ(deeper->inner.get()->get(1)->get(0)->inputs.get()->size(), 3) << "the second layer links the first";
	EXPECT_EQ(deeper->output.get()->get(0)->inputs.get()->size(), 3);
}

/** A 2-2-1 network with fixed weights: the forward pass, and two training epochs on one sample (PlayerModel.java:58-107,
 * PlayerModelLink.java:35-68) */
class FixedNetwork : public ::testing::Test {
protected:
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};

	void SetUp() override {
		model = PlayerModel::create(2, 2, 1, 1, 0.5, 0.25);
		in = neurons(*model->input.get());
		hid = neurons(*model->inner.get()->get(0));
		out = model->output.get()->get(0);
		for (size_t h = 0; h < 2; h++) {
			hid[h]->bias.set(b[h]);
			std::vector<runtime::Ptr<Link>> ls = links(*hid[h]->inputs.get());
			for (size_t i = 0; i < 2; i++)
				ls[i]->setWeight(w[h][i]);
		}
		out->bias.set(bo);
		std::vector<runtime::Ptr<Link>> ls = links(*out->inputs.get());
		for (size_t h = 0; h < 2; h++)
			ls[h]->setWeight(v[h]);
	}

	runtime::Ref<PlayerModel> model;
	std::vector<runtime::Ptr<PlayerModelLink>> in, hid;
	runtime::Ptr<PlayerModelLink> out;
	double w[2][2] = {{0.1, -0.2}, {0.3, 0.4}};
	double b[2] = {0.05, -0.15};
	double v[2] = {0.6, -0.7};
	double bo = 0.2;
};

TEST_F(FixedNetwork, TheForwardPass) {
	const std::vector<double> x = {1.0, 0.5};
	double h0 = sig(w[0][0] * x[0] + w[0][1] * x[1] + b[0]);
	double h1 = sig(w[1][0] * x[0] + w[1][1] * x[1] + b[1]);
	double o = sig(v[0] * h0 + v[1] * h1 + bo);
	EXPECT_EQ(model->getOutputEstimation(x), std::vector<double>{o});
	EXPECT_EQ(in[1]->value.get(), 0.5);
	EXPECT_EQ(hid[0]->value.get(), h0);
	EXPECT_EQ(hid[1]->value.get(), h1);
	EXPECT_EQ(PlayerModelController::getActionOutput(*model, x), 0);

	const std::vector<double> shortInput = {1.0};
	EXPECT_THROW(model->getOutputEstimation(shortInput), runtime::ArrayIndexOutOfBoundsException) << "Java: inputs[1] of a one-value array";
}

TEST_F(FixedNetwork, TrainingIsBackPropagationWithMomentum) {
	const double lr = 0.5, mom = 0.25, target = 0.25;
	const std::vector<double> x = {1.0, 0.5};
	double W[2][2] = {{w[0][0], w[0][1]}, {w[1][0], w[1][1]}}, B[2] = {b[0], b[1]}, V[2] = {v[0], v[1]}, BO = bo;
	double dW[2][2] = {}, dB[2] = {}, dV[2] = {}, dBO = 0;
	for (int epoch = 0; epoch < 2; epoch++) {
		double h[2];
		for (int k = 0; k < 2; k++)
			h[k] = sig(W[k][0] * x[0] + W[k][1] * x[1] + B[k]);
		double o = sig(V[0] * h[0] + V[1] * h[1] + BO);
		double go = (target - o) * (o * (1 - o));
		double gh[2];
		for (int k = 0; k < 2; k++)
			gh[k] = (go * V[k]) * (h[k] * (1 - h[k])); // the hidden gradients read the output weights before they change
		for (int k = 0; k < 2; k++) { // the hidden layer is updated first (PlayerModel.java:94-97)
			double prev = dB[k];
			dB[k] = lr * gh[k];
			B[k] += dB[k] + mom * prev;
			for (int i = 0; i < 2; i++) {
				prev = dW[k][i];
				dW[k][i] = lr * gh[k] * x[i];
				W[k][i] = W[k][i] + dW[k][i] + mom * prev;
			}
		}
		double prev = dBO;
		dBO = lr * go;
		BO += dBO + mom * prev;
		for (int k = 0; k < 2; k++) {
			prev = dV[k];
			dV[k] = lr * go * h[k];
			V[k] = V[k] + dV[k] + mom * prev;
		}
	}
	runtime::Ref<DataSet> sample = DataSet::create(x, std::vector<double>{target});
	EXPECT_EQ(sample->getValues(), x);
	EXPECT_EQ(sample->getTargets(), std::vector<double>{target});
	model->train({sample}, 2);
	EXPECT_TRUE(model->isReady.get());
	EXPECT_EQ(out->bias.get(), BO);
	EXPECT_EQ(out->biasDelta.get(), dBO);
	std::vector<runtime::Ptr<Link>> outLinks = links(*out->inputs.get());
	for (size_t k = 0; k < 2; k++) {
		EXPECT_EQ(outLinks[k]->getWeight(), V[k]) << k;
		EXPECT_EQ(outLinks[k]->getWeightDelta(), dV[k]) << k;
		EXPECT_EQ(hid[k]->bias.get(), B[k]) << k;
		std::vector<runtime::Ptr<Link>> ls = links(*hid[k]->inputs.get());
		for (size_t i = 0; i < 2; i++)
			EXPECT_EQ(ls[i]->getWeight(), W[k][i]) << k << i;
	}
}

/** PlayerModelEntry.java:105-170: the persisted values, the state input array (22 values and one per skill) and the action array */
TEST(PlayerModelTest, AnEntryFromPersistence) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	commons::database::Timestamp when{std::chrono::milliseconds(1700000000000)};
	runtime::Ref<PlayerModelEntry> e = PlayerModelEntry::create(42, when, 1001, 7, 80.5f, 60.25f, true, false, true, false, true, 3, 2, true,
		50.0f, 40.0f, true, 12.5f, false, true, false, true, false, 4, 5, false);
	EXPECT_EQ(e->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(e->getPlayerID(), 42);
	EXPECT_EQ(e->getTimestamp(), when);
	EXPECT_EQ(e->getSkillID(), 1001);
	EXPECT_EQ(e->getPlayerClassID(), 7);
	EXPECT_EQ(e->getTimeCDdone(), 0.0f);
	EXPECT_EQ(e->toStateInputArray({1000, 1001, 1002}, 1002),
		(std::vector<double>{80.5, 60.25, 1, 0, 1, 0, 1, 3, 2, 1, 50, 40, 1, 12.5, 0, 1, 0, 1, 0, 4, 5, 0, 0, 0, 1}));
	EXPECT_EQ(e->toActionOutputArray({1000, 1001, 1002}), (std::vector<double>{0, 1, 0}));
	EXPECT_EQ(e->toActionOutputArray({}), std::vector<double>{});
	e->setPersistentState(Persistable::PersistentState::NEW);
	EXPECT_EQ(e->getPersistentState(), Persistable::PersistentState::NEW);
}

/** PlayerModelEntry.java:40-102: the live entry of a player, without and with a target that targets him */
class PlayerModelEntryLiveTest : public InWorldPacketTest {
protected:
	void TearDown() override {
		if (actor.player)
			actor.player->setTarget(nullptr);
		if (other.player)
			other.player->setTarget(nullptr);
		actor = {};
		other = {};
		InWorldPacketTest::TearDown();
	}

	PlayerFixture actor, other;
};

TEST_F(PlayerModelEntryLiveTest, TheLiveValues) {
	actor = makePlayer(731501, 9701, "Modeled");
	other = makePlayer(731502, 9702, "Target");
	const int64_t before = commons::utils::currentTimeMillis();
	runtime::Ref<PlayerModelEntry> alone = PlayerModelEntry::create(*actor.player, 1234, nullptr);
	const int64_t after = commons::utils::currentTimeMillis();
	EXPECT_EQ(alone->getPersistentState(), Persistable::PersistentState::NEW);
	EXPECT_EQ(alone->getPlayerID(), 731501);
	EXPECT_EQ(alone->getSkillID(), 1234);
	EXPECT_EQ(alone->getPlayerClassID(), model::getClassId(model::PlayerClass::WARRIOR));
	EXPECT_EQ(alone->getPlayerHPpercentage(), static_cast<float>(actor.player->getLifeStats()->getHpPercentage()));
	EXPECT_EQ(alone->getTargetBuffCount(), -1);
	EXPECT_EQ(alone->getTargetDebuffCount(), -1);
	EXPECT_EQ(alone->getTargetHPpercentage(), -1.0f);
	EXPECT_EQ(alone->getTargetMPpercentage(), -1.0f);
	EXPECT_EQ(alone->getDistance(), -1.0f);
	EXPECT_FALSE(alone->isTargetFocusesPlayer());
	EXPECT_GE(alone->getTimestamp().time_since_epoch().count(), before) << "Java: new Timestamp(System.currentTimeMillis())";
	EXPECT_LE(alone->getTimestamp().time_since_epoch().count(), after);

	other.player->setPosition(world::WorldPosition::create(210010000, 103.0f, 104.0f, 50.0f, int8_t{0}));
	other.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	runtime::Ref<PlayerModelEntry> targeted = PlayerModelEntry::create(*actor.player, 1234, runtime::Ptr<model::gameobjects::Creature>(*other.player));
	EXPECT_TRUE(targeted->isTargetFocusesPlayer());
	EXPECT_EQ(targeted->getDistance(), 5.0f);
	EXPECT_EQ(targeted->getTargetBuffCount(), 0);
	EXPECT_EQ(targeted->getTargetDebuffCount(), 0);
	EXPECT_EQ(targeted->getTargetHPpercentage(), static_cast<float>(other.player->getLifeStats()->getHpPercentage()));
	other.player->setTarget(nullptr);
	EXPECT_FALSE(PlayerModelEntry::create(*actor.player, 1234, runtime::Ptr<model::gameobjects::Creature>(*other.player))->isTargetFocusesPlayer());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
