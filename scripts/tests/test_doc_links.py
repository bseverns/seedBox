"""Exercise the local-link guard against missing and nonportable destinations."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_doc_links import check_file


class DocLinksTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='seedbox-doc-links-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.doc = self.root / 'docs' / 'guide.md'
        self.doc.parent.mkdir()
        self.target = self.root / 'include' / 'Seed.h'
        self.target.parent.mkdir()
        self.target.write_text('// reference\n')
        self.known = {self.root, self.doc, self.doc.parent, self.target, self.target.parent}

    def check(self, text):
        self.doc.write_text(text)
        return check_file(self.root, self.doc, self.known)

    def test_relative_root_reference_and_html_links(self):
        self.assertEqual([], self.check('''[`Seed`](../include/Seed.h#L1)
![image](../include/Seed.h "title")
[root](/include/Seed.h)
[reference][seed]
[seed]: ../include/Seed.h
<a href="../include/Seed.h">header</a>
'''))

    def test_missing_wrong_case_and_untracked_targets(self):
        for destination in ('../missing.h', '../include/seed.h', '../build/output.wav'):
            with self.subTest(destination=destination):
                self.assertIn('missing repository target', self.check(f'[file]({destination})')[0])
        ignored = self.root / 'build' / 'output.wav'
        ignored.parent.mkdir()
        ignored.write_text('An ignored build output is not a repository link target.')
        self.assertIn('missing repository target', self.check('[render](../build/output.wav)')[0])

    def test_machine_paths_and_escape_are_rejected(self):
        for destination in ('/Users/person/repo/Seed.h', '/home/person/Seed.h',
                            '/tmp/output.wav', 'file:///tmp/output.wav', 'C:/repo/Seed.h',
                            '~/repo/Seed.h', 'vscode://file/tmp/Seed.h'):
            with self.subTest(destination=destination):
                self.assertIn('machine-specific', self.check(f'[file]({destination})')[0])
        self.assertIn('machine-specific', self.check('<file:///tmp/output.wav>')[0])
        self.assertIn('escapes repository', self.check('[file](../../outside.md)')[0])

    def test_fences_inline_code_external_urls_and_fragments_are_ignored(self):
        self.assertEqual([], self.check('''```md
[example](missing.md)
```
~~~
<img src="/Users/person/example.png">
~~~
`[example](missing.md)`
[external](https://example.org/missing)
[email](mailto:someone@example.org)
[fragment](#some-heading)
Receipt source: `/Users/person/input.wav`
'''))

    def test_encoded_and_parenthesized_paths(self):
        for name, destination in (('some file.md', '<some file.md>'),
                                  ('name#part.md', 'name%23part.md'),
                                  ('name(test).md', 'name(test).md')):
            target = self.doc.parent / name
            target.write_text('reference')
            self.known.add(target)
            self.assertEqual([], self.check(f'[file]({destination})'))

    def test_missing_reference_and_html_targets_report_lines(self):
        errors = self.check('Intro\n\n[ref]: missing.md\n<img src="absent.png">\n')
        self.assertEqual(2, len(errors))
        self.assertTrue(any('docs/guide.md:3:' in error for error in errors))
        self.assertTrue(any('docs/guide.md:4:' in error for error in errors))


if __name__ == '__main__':
    unittest.main()
