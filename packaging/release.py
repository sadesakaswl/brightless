#!/usr/bin/env python3
"""Verify this workflow's seven packages and publish its version as a GitHub release."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile


def prepare_assets(artifacts, output, version):
    names = {
        f"brightless-{version}-win64.zip": f"brightless-v{version}-windows-x64.zip",
        f"brightless-{version}-Linux.zip": f"brightless-v{version}-linux-x64.zip",
    }
    for suffix in ("-1-x86_64.pkg.tar.zst", "-1.fc44.x86_64.rpm", "-x86_64.AppImage", "-x86_64.flatpak"):
        name = f"brightless-{version}{suffix}"
        names[name] = name
    name = f"brightless_{version}-1_amd64.deb"
    names[name] = name
    output.mkdir(parents=True, exist_ok=True)
    checksums = []
    for source, destination in names.items():
        matches = list(artifacts.rglob(source))
        if len(matches) != 1 or matches[0].stat().st_size == 0:
            raise ValueError(f"Expected exactly one nonempty {source}, found {matches}")
        if source.endswith(".zip"):
            with zipfile.ZipFile(matches[0]) as archive:
                if archive.testzip() is not None:
                    raise ValueError(f"Corrupt ZIP: {source}")
        target = output / destination
        shutil.copyfile(matches[0], target)
        with target.open("rb") as file:
            digest = hashlib.file_digest(file, "sha256").hexdigest()
        checksums.append(f"{digest}  {destination}\n")
    (output / "SHA256SUMS").write_text("".join(checksums), encoding="utf-8")
    return [output / name for name in names.values()] + [output / "SHA256SUMS"]


def main():
    version = re.search(r"project\(brightless VERSION (\d+\.\d+\.\d+) ", Path("CMakeLists.txt").read_text()).group(1)
    tag = f"v{version}"
    if os.environ.get("GITHUB_REF", "").startswith("refs/tags/") and os.environ["GITHUB_REF_NAME"] != tag:
        raise ValueError("Release tag does not match CMakeLists.txt")
    notes = Path(f"releases/{version}.md")
    if not notes.is_file():
        raise ValueError(f"Missing release notes: {notes}")
    assets = prepare_assets(Path(sys.argv[1]), Path("build/release/upload"), version)
    sha = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    existing = subprocess.run(["gh", "release", "view", tag, "--json", "isDraft", "--jq", ".isDraft"], capture_output=True, text=True)
    if existing.returncode == 0:
        if existing.stdout.strip() != "true":
            raise ValueError(f"Refusing to overwrite published release {tag}")
        subprocess.run(["gh", "release", "edit", tag, "--target", sha, "--title", f"Brightless {version}", "--notes-file", str(notes)], check=True)
    else:
        subprocess.run(["gh", "release", "create", tag, "--draft", "--target", sha, "--title", f"Brightless {version}", "--notes-file", str(notes)], check=True)
    subprocess.run(["gh", "release", "upload", tag, "--clobber", *map(str, assets)], check=True)
    subprocess.run(["gh", "release", "edit", tag, "--draft=false", "--latest"], check=True)


if __name__ == "__main__":
    main()
