// Read the compiled contract rather than parsing C++ initializers in doc checks.
#include <iostream>
#include "hal/PanelControls.h"

int main() {
  for (const auto& control : hal::panel::encoders)
    std::cout << "encoder\t" << control.label << '\t' << control.token << '\t'
              << unsigned(control.pin_a) << '/' << unsigned(control.pin_b) << '/'
              << unsigned(control.pin_switch) << '\n';
  for (const auto& control : hal::panel::buttons)
    std::cout << "button\t" << control.label << '\t' << control.token << '\t'
              << unsigned(control.pin) << '\n';
}
