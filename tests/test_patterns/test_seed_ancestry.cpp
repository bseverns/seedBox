#include <unity.h>
#include "app/SeedAncestry.h"

void test_mutation_record_keeps_parent_and_moved_dimensions() {
  Seed parent{}; parent.id = 4; parent.prng = 9; parent.lineage = 12;
  Seed child = parent; child.pitch += 6.f; child.tone += .25f;
  seedbox::recordMutation(parent, child);
  TEST_ASSERT_EQUAL_UINT32(4, child.ancestry.parentId);
  TEST_ASSERT_EQUAL_UINT32(9, child.ancestry.parentPrng);
  TEST_ASSERT_EQUAL_UINT32(12, child.ancestry.parentLineage);
  TEST_ASSERT_TRUE(child.ancestry.dimensions & seedbox::kPitch);
  TEST_ASSERT_TRUE(child.ancestry.dimensions & seedbox::kTone);
  TEST_ASSERT_TRUE(child.ancestry.distance > 0.f && child.ancestry.distance <= 1.f);
}
