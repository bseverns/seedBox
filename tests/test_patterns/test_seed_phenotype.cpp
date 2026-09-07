#include <unity.h>

#include "Seed.h"
#include "app/SeedPhenotype.h"

void test_phenotype_is_bounded_and_preserves_seed_identity() {
  Seed seed{};
  seed.id = 7; seed.prng = 9; seed.source = Seed::Source::kPreset; seed.lineage = 11;
  seed.engine = 2; seed.sampleIdx = 4; seed.granular.source = 1; seed.granular.sdSlot = 3;
  seedbox::applyPhenotype(seed, {-2.f, 2.f, 2.f, -2.f, 2.f, -2.f, 2.f});
  TEST_ASSERT_EQUAL_UINT32(7, seed.id); TEST_ASSERT_EQUAL_UINT32(9, seed.prng);
  TEST_ASSERT_EQUAL_UINT32(11, seed.lineage); TEST_ASSERT_EQUAL_UINT8(2, seed.engine);
  TEST_ASSERT_EQUAL_UINT8(4, seed.sampleIdx); TEST_ASSERT_EQUAL_UINT8(1, seed.granular.source);
  TEST_ASSERT_EQUAL_UINT8(3, seed.granular.sdSlot);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, seed.density);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, seed.tone);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.05f, seed.spread);
}

void test_phenotype_correlates_energy_space_and_stability() {
  Seed quiet{}; Seed vivid{};
  seedbox::applyPhenotype(quiet, {0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f});
  seedbox::applyPhenotype(vivid, {1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f});
  TEST_ASSERT_TRUE(vivid.density > quiet.density);
  TEST_ASSERT_TRUE(vivid.tone > quiet.tone && vivid.spread > quiet.spread);
  TEST_ASSERT_TRUE(vivid.jitterMs > quiet.jitterMs && vivid.mutateAmt > quiet.mutateAmt);
  TEST_ASSERT_TRUE(vivid.envR > quiet.envR && vivid.resonator.feedback > quiet.resonator.feedback);
}

void test_phenotype_mapping_is_deterministic() {
  Seed a{}; Seed b{};
  const seedbox::SeedPhenotype phenotype{0.7f, 0.3f, 0.8f, 0.6f, 0.4f, 0.9f, 0.2f};
  seedbox::applyPhenotype(a, phenotype); seedbox::applyPhenotype(b, phenotype);
  TEST_ASSERT_EQUAL_FLOAT(a.density, b.density); TEST_ASSERT_EQUAL_FLOAT(a.pitch, b.pitch);
  TEST_ASSERT_EQUAL_FLOAT(a.granular.sprayMs, b.granular.sprayMs);
  TEST_ASSERT_EQUAL_FLOAT(a.resonator.feedback, b.resonator.feedback);
}
