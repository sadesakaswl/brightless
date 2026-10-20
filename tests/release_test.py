#!/usr/bin/env python3
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ci"))
import release
from release import prepare_assets


class ReleaseTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.artifacts = self.root / "artifacts"
        self.artifacts.mkdir()
        for name in ("brightless-0.5.0-win64.zip", "brightless-0.5.0-Linux.zip"):
            with zipfile.ZipFile(self.artifacts / name, "w") as archive:
                archive.writestr("brightless", "binary")
        for suffix in ("-1-x86_64.pkg.tar.zst", "-1.fc44.x86_64.rpm", "-x86_64.AppImage", "-x86_64.flatpak"):
            (self.artifacts / f"brightless-0.5.0{suffix}").write_bytes(b"package")
        (self.artifacts / "brightless_0.5.0-1_amd64.deb").write_bytes(b"package")

    def test_names_and_checksums(self):
        assets = prepare_assets(self.artifacts, self.root / "output", "0.5.0")
        self.assertEqual(len(assets), 8)
        self.assertIn("brightless-v0.5.0-windows-x64.zip", [p.name for p in assets])
        lines = assets[-1].read_text().splitlines()
        self.assertEqual(len(lines), 7)
        for line in lines:
            digest, name = line.split("  ")
            self.assertEqual(digest, hashlib.sha256((assets[-1].parent / name).read_bytes()).hexdigest())

    def test_missing_or_duplicate_package_is_rejected(self):
        package = self.artifacts / "brightless_0.5.0-1_amd64.deb"
        package.unlink()
        with self.assertRaises(ValueError):
            prepare_assets(self.artifacts, self.root / "output", "0.5.0")
        package.write_bytes(b"package")
        (self.artifacts / "duplicate").mkdir()
        (self.artifacts / "duplicate" / package.name).write_bytes(b"package")
        with self.assertRaises(ValueError):
            prepare_assets(self.artifacts, self.root / "output", "0.5.0")

    def test_published_release_is_not_modified(self):
        with (
            patch.object(release, "prepare_assets", return_value=[]),
            patch.object(release.subprocess, "check_output", return_value="abc123\n"),
            patch.object(release.subprocess, "run") as run,
            patch.object(sys, "argv", ["release.py", str(self.artifacts)]),
            patch.dict("os.environ", {"GITHUB_REF": "refs/heads/main"}),
        ):
            run.return_value.returncode = 0
            run.return_value.stdout = "false\n"
            release.main()
            run.assert_called_once()
            self.assertEqual(run.call_args.args[0][:3], ["gh", "release", "view"])

    def test_corrupt_zip_is_rejected(self):
        (self.artifacts / "brightless-0.5.0-win64.zip").write_bytes(b"not a ZIP")
        with self.assertRaises(zipfile.BadZipFile):
            prepare_assets(self.artifacts, self.root / "output", "0.5.0")


if __name__ == "__main__":
    unittest.main()
