#include <cstdio>

void test_scale_quantizer_snap_to_scale_major();
void test_scale_quantizer_snap_up_directional();
void test_scale_quantizer_snap_down_directional();
void test_scale_quantizer_root_wraps();

int main(int, char**) {
  std::puts("[scale_quantizer] running snap-to-scale scenarios...");
  test_scale_quantizer_snap_to_scale_major();
  test_scale_quantizer_snap_up_directional();
  test_scale_quantizer_snap_down_directional();
  test_scale_quantizer_root_wraps();
  std::puts("[scale_quantizer] all assertions passed.");
  return 0;
}
