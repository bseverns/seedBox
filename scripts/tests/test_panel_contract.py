"""Prove the guard rejects realistic drift, including malformed C++ contracts."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_panel_contract import ROOT, check_surfaces, load_contract


class PanelContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.controls = load_contract(ROOT)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="seedbox-panel-mutation-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in ("include/hal/Board.h", "include/hal/PanelControls.h", "tools/panel_contract_dump.cpp",
                     "docs/panel_cheat_sheet.md", "docs/hardware_bill_of_materials.md",
                     "docs/builder_bootstrap.md", "assets/front-panel-map.svg"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)

    def mutate(self, name, before, after):
        path = self.root / name
        text = path.read_text()
        self.assertIn(before, text)
        path.write_text(text.replace(before, after, 1))

    def test_current_surfaces_match(self):
        self.assertEqual([], check_surfaces(self.root, self.controls))

    def test_rejects_surface_drift(self):
        mutations = [
            ("docs/panel_cheat_sheet.md", "`capture`", "`reseed`"),
            ("docs/panel_cheat_sheet.md", "| 34 |", "| 35 |"),
            ("docs/hardware_bill_of_materials.md", "Momentary push buttons (x4)", "Momentary push buttons (x5)"),
            ("docs/builder_bootstrap.md", "Live Capture: 34", "Live Capture: 35"),
            ("assets/front-panel-map.svg", '>Live Capture</text>', '>Reseed</text>'),
            ("assets/front-panel-map.svg", 'id="capture"', 'id="lock"'),
            ("assets/front-panel-map.svg", '34 · Hold: panic', '35 · Hold: panic'),
            ("assets/front-panel-map.svg", '</svg>', '<circle class="button"/></svg>'),
        ]
        for name, before, after in mutations:
            with self.subTest(name=name, change=after):
                original = (self.root / name).read_text()
                self.mutate(name, before, after)
                self.assertTrue(check_surfaces(self.root, self.controls))
                (self.root / name).write_text(original)

    def test_rejects_invalid_compiled_contracts(self):
        mutations = [
            ("include/hal/PanelControls.h", "Board::EncoderID::Density}", "Board::EncoderID::ToneTilt}", "Panel switches must map"),
            ("include/hal/PanelControls.h", "LiveCapture, 34", "LiveCapture, 3", "Panel GPIO pins must be unique"),
            ("include/hal/PanelControls.h", 'LiveCapture, 34', 'Shift, 34', 'Panel switches must map'),
            ("include/hal/PanelControls.h", '"capture"', '"shift"', 'Panel labels and lowercase'),
            ("include/hal/PanelControls.h", '"Live Capture"', '""', 'Panel labels and lowercase'),
            ("include/hal/PanelControls.h", 'array<Button, 4>', 'array<Button, 5>', 'Panel button count'),
            ("include/hal/Board.h", '    LiveCapture,', '    LiveCapture,\n    UnmappedButton,', 'Panel button count'),
            ("include/hal/Board.h", '    FxMutate,', '    FxMutate,\n    UnmappedEncoder,', 'Panel encoder count'),
        ]
        for name, before, after, diagnostic in mutations:
            with self.subTest(change=after):
                original = (self.root / name).read_text()
                self.mutate(name, before, after)
                # Compile in a child process so diagnostics can be asserted without noise.
                result = subprocess.run([sys.executable, str(ROOT / "scripts/check_panel_contract.py"),
                                         "--root", str(self.root)], capture_output=True, text=True)
                self.assertNotEqual(0, result.returncode)
                self.assertIn(diagnostic, result.stderr)
                (self.root / name).write_text(original)


if __name__ == "__main__":
    unittest.main()
