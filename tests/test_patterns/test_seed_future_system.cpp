#include <unity.h>
#include "app/EngineEcology.h"
#include "app/SeedLineage.h"

void test_branch_return_and_semantic_cross_are_deterministic() {
  Seed seed{}; seed.id = 9; seed.pitch = 3.f;
  seedbox::SeedBranch branch; branch.forkFrom(seed); branch.current.pitch = 10.f;
  TEST_ASSERT_TRUE(branch.returnToAncestor()); TEST_ASSERT_EQUAL_FLOAT(3.f, branch.current.pitch);
  seedbox::SeedPhenotype a{.2f,.8f,.3f,.4f,.1f,.7f,.2f}, b{.8f,.2f,.9f,.6f,.9f,.3f,.8f};
  const auto one = seedbox::crossPhenotypes(a,b,42), two = seedbox::crossPhenotypes(a,b,42);
  TEST_ASSERT_EQUAL_FLOAT(.5f, one.energy); TEST_ASSERT_EQUAL_FLOAT(one.roughness, two.roughness);
}

void test_burst_ecology_only_excites_resonator() {
  Seed seed{}; seed.pitch = 4.f; seed.resonator.brightness = .4f;
  const Seed excited = seedbox::burstExcitesResonator(seed, 1.f);
  TEST_ASSERT_EQUAL_FLOAT(4.f, excited.pitch);
  TEST_ASSERT_TRUE(excited.resonator.exciteMs > seed.resonator.exciteMs);
  TEST_ASSERT_TRUE(excited.resonator.brightness > seed.resonator.brightness);
}
