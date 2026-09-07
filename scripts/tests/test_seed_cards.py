"""Test Seed Card validation against a complete temporary proof."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validate_seed_cards import validate_registry


class SeedCardValidationTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="seedbox-seed-card-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "docs/seed_cards/cards").mkdir(parents=True)
        (self.root / "build/fixtures").mkdir(parents=True)
        self.audio = self.root / "build/fixtures/proof.wav"
        self.ledger = self.root / "build/fixtures/proof-control.txt"
        self.audio.write_bytes(b"RIFF test evidence")
        self.ledger.write_text("frame=0 control=seed value=42\n")

    def write_card(self, mutate=None):
        card = {
            "format_version": "1.0", "id": "proof-seed", "name": "Proof Seed",
            "seed": {"master_seed": 42, "id": 0, "prng": 9, "source": "lfsr", "lineage": 42,
                     "genome": {"pitch": 0.0, "envA": 0.001, "envD": 0.08, "envS": 0.6, "envR": 0.12,
                                "density": 1.0, "probability": 0.85, "jitterMs": 7.5, "tone": 0.35, "spread": 0.2,
                                "engine": 0, "sampleIdx": 0, "mutateAmt": 0.1,
                                "granular": {"grainSizeMs": 90.0, "sprayMs": 18.0, "transpose": 0.0, "windowSkew": 0.0, "stereoSpread": 0.5, "source": 0, "sdSlot": 0},
                                "resonator": {"exciteMs": 3.5, "damping": 0.35, "brightness": 0.6, "feedback": 0.78, "mode": 0, "bank": 0}}},
            "controls": [{"control": "Seed", "value": 42, "purpose": "Pins the deterministic sequence."}],
            "body": {"kind": "native", "target": "native golden", "revision": "abc123"},
            "evidence": {"audio_render": {"path": "build/fixtures/proof.wav", "description": "Audio render."},
                         "control_ledger": {"path": "build/fixtures/proof-control.txt", "description": "Control ledger."}},
            "hashes": {"algorithm": "sha256", "audio_render": hashlib.sha256(self.audio.read_bytes()).hexdigest(),
                       "control_ledger": hashlib.sha256(self.ledger.read_bytes()).hexdigest()},
            "comparison": {"changed": "Density moved from the baseline.", "stayed_fixed": "Body and master seed stayed fixed."},
        }
        if mutate:
            mutate(card)
        (self.root / "docs/seed_cards/cards/proof-seed.json").write_text(json.dumps(card))
        (self.root / "docs/seed_cards/index.json").write_text(json.dumps({"format_version": "1.0", "cards": ["docs/seed_cards/cards/proof-seed.json"]}))

    def test_complete_card_with_matching_evidence_passes(self):
        self.write_card()
        self.assertEqual([], validate_registry(self.root))

    def test_hash_mismatch_and_missing_proof_field_fail(self):
        def mutate(card):
            card["hashes"]["audio_render"] = "0" * 64
            del card["comparison"]["stayed_fixed"]
        self.write_card(mutate)
        errors = validate_registry(self.root)
        self.assertTrue(any("does not match" in error for error in errors))
        self.assertTrue(any("stayed_fixed" in error for error in errors))

    def test_unregistered_card_fails(self):
        self.write_card()
        (self.root / "docs/seed_cards/index.json").write_text('{"format_version": "1.0", "cards": []}')
        self.assertTrue(any("not listed" in error for error in validate_registry(self.root)))

    def test_evidence_path_cannot_escape_repository(self):
        self.write_card(lambda card: card["evidence"]["audio_render"].update(path="../outside.wav"))
        self.assertTrue(any("path escapes repository" in error for error in validate_registry(self.root)))


if __name__ == "__main__":
    unittest.main()
