#!/usr/bin/env python3
"""Writes this mod's online symbol cache from Windhawk's ported symbols.

Windhawk finds the symbols a mod hooks in its own cache, then in an online
cache, and only then in the module's symbol file (PDB), which it downloads
from Microsoft. Its online cache, built by ramensoftware/windhawk-mod-symbol-cache,
only covers the mods in Windhawk's catalog. When Microsoft publishes a Windows
update's symbol files late, that project ports the symbols of the updated
modules from earlier builds into "ported symbols" files, so the catalog mods
keep working. This mod, which Windhawk doesn't look up there, failed to load
until the PDBs were out.

For each ported symbols file of a module the mod hooks, this script writes
the mod's symbols in the online cache's format to
<output>/<mod id>/pdb_<fingerprint>.txt. HookSymbolsTai points Windhawk at that
folder on the repository's symbol-cache branch, which
.github/workflows/symbol-cache.yml keeps up to date.

A symbol the ported file has is written with its offset, and one it lists as
absent with an empty one, which tells Windhawk that the symbol doesn't exist.
A symbol it doesn't mention, or lists more than once, is left out, so Windhawk
looks for it in the PDB, as it does for the catalog mods. A cache file whose
ported file is gone is removed once Microsoft has published the PDB, from
which Windhawk then reads the symbols as for any other build.

Needs Python 3.14 or later (compression.zstd) and nothing outside the standard
library. Set GITHUB_TOKEN to raise GitHub's API rate limit.
"""

import argparse
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

try:
    from compression import zstd
except ImportError:
    sys.exit('Python 3.14 or later is needed, for compression.zstd.')

SOURCE_REPO = 'ramensoftware/windhawk-mod-symbol-cache'
SOURCE_BRANCH = 'main'

# The modules whose symbols the mod hooks, that is the modules it passes to
# HookSymbolsTai. A module left out here is still hooked, from its PDB.
MODULES = (
    'taskbar.dll',
    'taskbar.view.dll',
    'systemtray.dll',
    'searchux.ui.dll',
    'startdocked.dll',
    'twinui.pcshell.dll',
)

# @architecture value: (folder of the ported symbols, tag in a cache file).
ARCHITECTURES = {
    'x86': ('x86', 'x86'),
    'x86-64': ('amd64', 'x86-64'),
    'amd64': ('amd64', 'x86-64'),
    'arm64': ('arm64', 'arm64'),
}

CACHE_SEPARATOR = '#'

OUTPUT_README = """\
# Online symbol cache of TAI (vo fork)

Written by `tools/update-symbol-cache.py` on the `main` branch, from the
ported symbols of [windhawk-mod-symbol-cache](https://github.com/ramensoftware/windhawk-mod-symbol-cache).
Don't edit it by hand.

When Microsoft publishes a Windows update's symbol files late, Windhawk can't
find the functions the mod hooks in the updated Windows files. The mod points
Windhawk at `taskbar-dock-like-vo/` here, and each `pdb_<fingerprint>.txt`
file holds the mod's symbols for one such file, in the format of Windhawk's
own online symbol cache. A file is removed once Microsoft has published the
symbols, which Windhawk then downloads as for any other update.
"""


def log(message: str):
    print(message, flush=True)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def http_get(url: str, headers: dict[str, str] | None = None,
             follow_redirects: bool = True) -> tuple[int, bytes]:
    """The status and body of a GET request, retried on network errors and
    server errors. A 4xx status, or a redirect when follow_redirects is off,
    is returned rather than raised."""
    request_headers = {'User-Agent': 'update-symbol-cache'}
    request_headers.update(headers or {})
    opener = (urllib.request.build_opener() if follow_redirects
              else urllib.request.build_opener(NoRedirect))
    attempts = 4
    for attempt in range(attempts):
        try:
            request = urllib.request.Request(url, headers=request_headers)
            with opener.open(request, timeout=60) as response:
                return response.status, response.read()
        except urllib.error.HTTPError as e:
            retry = e.code >= 500 or e.code == 429
            if not retry or attempt + 1 == attempts:
                return e.code, e.read()
        except (urllib.error.URLError, TimeoutError, ConnectionError):
            if attempt + 1 == attempts:
                raise
        time.sleep(5 * (attempt + 1))
    raise AssertionError('unreachable')


def github_api(path: str) -> dict:
    headers = {
        'Accept': 'application/vnd.github+json',
        'X-GitHub-Api-Version': '2022-11-28',
    }
    token = os.environ.get('GITHUB_TOKEN')
    if token:
        headers['Authorization'] = f'Bearer {token}'
    status, body = http_get('https://api.github.com' + path, headers)
    if status != 200:
        raise RuntimeError(f'GitHub API {path}: HTTP {status}: {body[:200]!r}')
    return json.loads(body)


def read_mod(mod_path: Path):
    """The mod's ID, its folder and tag per architecture, and its symbols in
    the order they first appear."""
    text = mod_path.read_text(encoding='utf-8')

    match = re.search(r'^//[ \t]*@id[ \t]+(\S+)', text, re.M)
    if not match:
        raise SystemExit(f'{mod_path}: no @id')
    mod_id = match[1]

    architectures = re.findall(r'^//[ \t]*@architecture[ \t]+(\S+)', text, re.M)
    if not architectures:
        architectures = ['x86', 'x86-64', 'arm64']
    unknown = [a for a in architectures if a not in ARCHITECTURES]
    if unknown:
        raise SystemExit(f'{mod_path}: unknown @architecture {unknown}')
    architectures = {ARCHITECTURES[a][0]: ARCHITECTURES[a][1] for a in architectures}

    symbols = []
    for symbol in dict.fromkeys(re.findall(r'LR"\((.*?)\)"', text, re.S)):
        if CACHE_SEPARATOR in symbol or not symbol.isascii():
            log(f'warning: skipping a symbol the cache format can\'t hold: {symbol[:80]}')
            continue
        symbols.append(symbol)

    lowered = text.lower()
    for module in MODULES:
        if f'"{module}"' not in lowered:
            log(f'warning: {module} is in MODULES but the mod never names it')

    return mod_id, architectures, symbols


def list_ported_files(architectures: dict[str, str]):
    """The source commit, and the ported symbols files of the modules and
    architectures the mod uses, as (module, architecture, path) tuples."""
    ref = github_api(f'/repos/{SOURCE_REPO}/git/ref/heads/{SOURCE_BRANCH}')
    commit = ref['object']['sha']
    tree = github_api(f'/repos/{SOURCE_REPO}/git/trees/{commit}?recursive=1')
    if tree.get('truncated'):
        raise RuntimeError('The source repository tree listing is truncated')

    files = []
    for entry in tree['tree']:
        match = re.fullmatch(r'ported_symbols/([^/]+)/([^/]+)/[^/]+\.txt\.zst', entry['path'])
        if (entry['type'] == 'blob' and match and match[1].lower() in MODULES
                and match[2] in architectures):
            files.append((match[1].lower(), match[2], entry['path']))
    return commit, sorted(files)


def read_ported_file(data: bytes):
    """The PE information of a ported symbols file, and the address of each
    symbol in it: an int, '' for a symbol listed as absent, or None for one
    listed more than once."""
    info: dict[str, str] = {}
    addresses: dict[str, int | str | None] = {}
    for line in zstd.decompress(data).splitlines():
        if not line:
            continue
        if line.startswith(b'['):
            end = line.find(b'] ')
            if end < 2:
                raise ValueError(f'Bad symbol line: {line[:100]!r}')
            symbol = line[end + 2:].decode('utf-8')
            value: int | str = int(line[1:end], 16)
        elif line.startswith(b'absent: '):
            symbol = line[len(b'absent: '):].decode('utf-8')
            value = ''
        else:
            match = re.fullmatch(rb'(\w+)=(.*)', line)
            if match:
                info[match[1].decode()] = match[2].decode()
                continue
            if line.startswith(b'Found '):
                continue
            raise ValueError(f'Unknown line: {line[:100]!r}')
        addresses[symbol] = None if symbol in addresses else value
    return info, addresses


def cache_file_content(module: str, arch_tag: str, info: dict[str, str],
                       symbols: list[str], addresses: dict):
    """The cache file for the mod's symbols, in the format of Windhawk's
    online symbol cache, with counts of the symbols found, absent and listed
    more than once. The second and third values are only labels for the
    engine, which ignores them."""
    parts = ['1', module, f'{arch_tag}_{info["timestamp"]}_{info["image_size"]}_ported']
    found = absent = duplicate = 0
    for symbol in symbols:
        if symbol not in addresses:
            continue
        value = addresses[symbol]
        if value is None:
            duplicate += 1
            continue
        parts += [symbol, str(value)]
        if value == '':
            absent += 1
        else:
            found += 1
    if found + absent == 0:
        return None, 0, 0, duplicate
    return CACHE_SEPARATOR.join(parts), found, absent, duplicate


def pdb_published(module: str, fingerprint: str) -> bool:
    """Whether Microsoft's symbol server has the PDB. Its paths are case
    insensitive, and a Windows module's PDB is named after the module."""
    pdb = re.sub(r'\.[^.]+$', '', module) + '.pdb'
    status, _ = http_get(
        f'https://msdl.microsoft.com/download/symbols/{pdb}/{fingerprint}/{pdb}',
        {'User-Agent': 'Microsoft-Symbol-Server/10.0.0.0'},
        follow_redirects=False)
    if status in (200, 301, 302, 303, 307, 308):
        return True
    if status == 404:
        return False
    raise RuntimeError(f'Symbol server: HTTP {status} for {pdb}/{fingerprint}')


def write_if_changed(path: Path, content: bytes) -> bool:
    if path.exists() and path.read_bytes() == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--mod', type=Path,
                        default=Path(__file__).resolve().parent.parent / 'taskbar-dock-like.wh.cpp',
                        help='the assembled mod (default: taskbar-dock-like.wh.cpp)')
    parser.add_argument('--output', type=Path, required=True,
                        help='a checkout of the symbol-cache branch')
    parser.add_argument('--commit-message-file', type=Path,
                        help='where to write a commit message, if anything changed')
    args = parser.parse_args()

    mod_id, architectures, symbols = read_mod(args.mod)
    log(f'{args.mod.name}: mod {mod_id}, {len(symbols)} symbol strings, '
        f'architectures {", ".join(architectures)}')

    commit, ported_files = list_ported_files(architectures)
    log(f'{SOURCE_REPO}@{commit[:12]}: {len(ported_files)} ported symbols files '
        f'for the mod\'s modules')

    cache_dir = args.output / mod_id
    written: set[Path] = set()
    changes: list[str] = []
    summary: list[str] = []

    for module, arch, path in ported_files:
        status, data = http_get(f'https://raw.githubusercontent.com/{SOURCE_REPO}/{commit}/{path}')
        if status != 200:
            raise RuntimeError(f'{path}: HTTP {status}')
        info, addresses = read_ported_file(data)
        if 'is_hybrid' in info:
            log(f'{path}: skipped, a hybrid module')
            continue
        fingerprint = info.get('pdb_fingerprint')
        if not fingerprint or not re.fullmatch(r'[0-9A-Fa-f]{33,40}', fingerprint):
            log(f'{path}: skipped, no PDB fingerprint')
            continue

        content, found, absent, duplicate = cache_file_content(
            module, architectures[arch], info, symbols, addresses)
        cache_file = cache_dir / f'pdb_{fingerprint}.txt'
        line = (f'{module} {info["timestamp"]}-{info["image_size"]} pdb_{fingerprint}: '
                f'{found} found, {absent} absent'
                + (f', {duplicate} listed more than once (left out)' if duplicate else '')
                + f', ported from {info.get("ported", "?")}')
        if content is None:
            log(f'{line}: none of the mod\'s symbols, no cache file')
            continue
        written.add(cache_file)
        if write_if_changed(cache_file, content.encode('ascii')):
            changes.append(f'{line}: written')
            log(f'{line}: written')
        else:
            log(f'{line}: unchanged')
        summary.append(f'| {module} | `pdb_{fingerprint}` | {found} | {absent} | {duplicate} |')

    # A cache file whose ported symbols are gone stays until its PDB is out.
    if cache_dir.is_dir():
        for cache_file in sorted(cache_dir.glob('pdb_*.txt')):
            if cache_file in written:
                continue
            fingerprint = cache_file.stem.removeprefix('pdb_')
            module = cache_file.read_text(encoding='ascii').split(CACHE_SEPARATOR)[1]
            if pdb_published(module, fingerprint):
                cache_file.unlink()
                changes.append(f'{module} pdb_{fingerprint}: removed, its PDB is published')
                log(changes[-1])
            else:
                log(f'{module} pdb_{fingerprint}: kept, its ported symbols are gone '
                    f'but its PDB isn\'t published')

    # The cache files hold no line breaks, which keeps the engine's parse
    # exact: a trailing one would become the last symbol's offset.
    for name, content in (('README.md', OUTPUT_README.encode('ascii')),
                          ('.gitattributes', b'* -text\n')):
        if write_if_changed(args.output / name, content):
            changes.append(f'{name}: written')

    if args.commit_message_file and changes:
        args.commit_message_file.write_text(
            'Update the symbol cache\n\n' + '\n'.join(changes)
            + f'\n\nSource: {SOURCE_REPO}@{commit}\n', encoding='utf-8')

    step_summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if step_summary:
        with open(step_summary, 'a', encoding='utf-8') as f:
            f.write(f'Source: `{SOURCE_REPO}@{commit[:12]}`\n\n')
            if summary:
                f.write('| Module | Cache file | Found | Absent | Left out |\n'
                        '| --- | --- | --- | --- | --- |\n' + '\n'.join(summary) + '\n\n')
            f.write('\n'.join(f'- {c}' for c in changes) if changes else 'No changes.')
            f.write('\n')

    log(f'{len(changes)} change(s)')


if __name__ == '__main__':
    main()
