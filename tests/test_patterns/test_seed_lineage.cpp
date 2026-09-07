#include <unity.h>
#include "app/SeedLineage.h"

void test_siblings_are_deterministic_and_nearer_than_unrelated_phenotypes() {
  const seedbox::SeedPhenotype parent{.65f, .70f, .55f, .60f, .35f, .75f, .45f};
  const auto sibling = seedbox::siblingPhenotype(parent, 0x51EEDu, .12f);
  const auto repeat = seedbox::siblingPhenotype(parent, 0x51EEDu, .12f);
  const seedbox::SeedPhenotype unrelated{.15f, .15f, .90f, .10f, .90f, .10f, .90f};
  TEST_ASSERT_EQUAL_FLOAT(sibling.energy, repeat.energy);
  TEST_ASSERT_EQUAL_FLOAT(sibling.roughness, repeat.roughness);
  TEST_ASSERT_TRUE(seedbox::phenotypeDistance(parent, sibling) < seedbox::phenotypeDistance(parent, unrelated));
}
