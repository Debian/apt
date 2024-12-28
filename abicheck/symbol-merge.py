"""
Download packages from the archive and generate a symbols file for them.
"""

import argparse
import glob
import os
import subprocess
import re
import sys
import urllib.request

import distro_info

import apt_pkg
import apt_inst

import apt.progress.text

STDLIB = (
    "^typeinfo for std::"
    "|^vtable for std::"
    "|^typeinfo name for std::"
    "|^guard variable for std::"
    "|^std::"
    "|^[a-z]* std::"
    "|^typeinfo for __gnu_cxx::"
    "|^vtable for __gnu_cxx::"
    "|^typeinfo name for __gnu_cxx::"
    "|^guard variable for __gnu_cxx::"
    "|^__gnu_cxx::"
    "|^[a-z]* __gnu_cxx::"
)
STDLIB_RE = re.compile(STDLIB)


def get_archs(dist: str) -> set[str]:
    if dist in distro_info.UbuntuDistroInfo().all:
        url = f"https://snapshot.ubuntu.com/ubuntu/dists/{dist}/InRelease"
    else:
        url = f"https://deb.debian.org/debian/dists/{dist}/InRelease"
    with urllib.request.urlopen(url) as rel:
        for line in rel:
            line = line.decode("utf-8")
            if line.startswith("Architectures:"):
                return set(line.split(":")[1].split()) - {"all"}
    raise ValueError("Invalid Release file")


def download_debs(dist: str) -> list[str]:
    """Download the debs and return the list of filenames."""
    runtime_dir = os.getenv("XDG_RUNTIME_DIR")
    if runtime_dir is None:
        raise ValueError("Need to set XDG_RUNTIME_DIR")
    tmpdir = f"{runtime_dir}/apt/symbol-merger"
    os.makedirs(f"{tmpdir}/etc/apt/sources.list.d", exist_ok=True)
    os.makedirs(f"{tmpdir}/etc/apt/apt.conf.d", exist_ok=True)
    os.makedirs(f"{tmpdir}/etc/apt/trusted.gpg.d", exist_ok=True)
    os.makedirs(f"{tmpdir}/var/lib/apt/lists/partial", exist_ok=True)
    os.makedirs(f"{tmpdir}/var/cache/apt/archives/partial", exist_ok=True)

    if dist in distro_info.UbuntuDistroInfo().all:
        url = "https://snapshot.ubuntu.com/ubuntu/"
        keyring = "/usr/share/keyrings/ubuntu-archive-keyring.gpg"
        suites = f"{dist} {dist}-updates"
    else:
        url = "https://deb.debian.org/debian/"
        keyring = "/usr/share/keyrings/debian-archive-keyring.gpg"
        suites = f"{dist}"

    with open(f"{tmpdir}/etc/apt/sources.list.d/debian.sources", "w") as sources:
        print("Types: deb", file=sources)
        print(f"URIs: {url}", file=sources)
        print(f"Suites: {suites}", file=sources)
        print("Components: main", file=sources)
        print(f"Signed-By: {keyring}", file=sources)
        print(f"Architectures: {" ".join(archs)}", file=sources)
        print("Targets: Packages", file=sources)
        print("", file=sources)

    apt_pkg.config.set("Dir", tmpdir)
    apt_pkg.config.set("Dir::State::status", "/dev/null")
    apt_pkg.config.set("APT::List-Cleanup", "false")
    apt_pkg.init()
    cache = apt_pkg.Cache()
    sl = apt_pkg.SourceList()
    sl.read_main_list()
    cache.update(apt.progress.text.AcquireProgress(), sl, 500000)
    cache = apt_pkg.Cache()
    depcache = apt_pkg.DepCache(cache)

    for arch in archs:
        for _, _, ver in cache[("libapt-pkg", arch)].provides_list:
            print("Mark", ver.parent_pkg.get_fullname(False), "for", arch, ver.ver_str)
            depcache.mark_install(ver.parent_pkg, False)

    acq = apt_pkg.Acquire(apt.progress.text.AcquireProgress())
    pm = apt_pkg.PackageManager(depcache)
    recs = apt_pkg.PackageRecords(cache)
    pm.get_archives(acq, sl, recs)
    acq.run()

    return [itm.destfile for itm in acq.items]


def read_main_symbols() -> dict[str, str]:
    """Read the versions of already known symbols."""
    symbols = {}
    with open(glob.glob("debian/libapt-pkg*.symbols")[0]) as sf:
        for line in sf:
            if not line.startswith(" "):
                if not symbols:
                    prelude.append(line.strip())
                continue
            if line.startswith(" "):
                symbol, version = line.strip().rsplit(None, 1)
                if '"' in symbol:
                    print(symbol.strip().split('"'))
                    prefix="(c++|optional=std)" if "optional=std" in symbol else "(c++)"
                    symbol = '{}"{}"'.format(prefix, symbol.strip().split('"')[1])
                symbols[symbol] = version
                # Register the symbol so we keep the ordering later.
                print("SYMBOL", symbol)
                symbol_archs[symbol] = set()
    return symbols


def read_symbol_file(debname: str) -> None:
    deb = apt_inst.DebFile(debname)
    arch = debname.split("_")[-1].split(".")[0]
    symbols = deb.control.extractdata("symbols").decode("utf-8")
    decoded_symbols = subprocess.Popen(
        ["c++filt"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True
    ).communicate(symbols)[0]
    for line, decoded_line in zip(symbols.splitlines(), decoded_symbols.splitlines()):
        if not line.startswith(" "):
            continue
        symbol, version = decoded_line.strip().rsplit(None, 1)
        symbol = symbol.strip()
        # Our version is higher than the registered one, so lower it again
        if line != decoded_line:
            if STDLIB_RE.search(symbol):
                symbol = '(c++|optional=std)"{}"'.format(symbol)
            else:
                symbol = '(c++)"{}"'.format(symbol)
        if (
            symbol not in smallest_symbol_version
            or apt_pkg.version_compare(version, smallest_symbol_version[symbol]) < 0
        ):
            smallest_symbol_version[symbol] = version

        try:
            symbol_archs[symbol].add(arch)
        except KeyError:
            symbol_archs[symbol] = {arch}
        try:
            latest_symbol_archs[symbol].add(arch)
        except KeyError:
            latest_symbol_archs[symbol] = {arch}


def print_merged() -> None:
    for line in prelude:
        print(line)

    for optional in False, True:
        if optional:
            print("# Optional C++ standard library symbols")
            print("# These are inlined libstdc++ symbols and not supposed to be part of our ABI")
            print("# but we cannot stop stuff from linking against it, sigh.")

        for symbol, line_archs in symbol_archs.items():
            if optional != ("optional=std" in symbol):
                continue
            if archs == line_archs:
                print("", symbol, smallest_symbol_version[symbol])

        for symbol, line_archs in symbol_archs.items():
            if optional != ("optional=std" in symbol):
                continue
            if archs != line_archs and line_archs:
                print_archs = " ".join(sorted(line_archs))
                if len(line_archs) > len(archs - line_archs):
                    print_archs = " ".join("!" + a for a in sorted(archs - line_archs))

                if "(c++" in symbol:
                    print(
                        "",
                        symbol.replace("(c++", f"(arch={print_archs}|c++"),
                        smallest_symbol_version[symbol],
                    )
                else:
                    print(
                        f" (arch={print_archs}){symbol} {smallest_symbol_version[symbol]}"
                    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("dist", nargs="+")
    args = parser.parse_args()
    prelude: list[str] = []
    symbol_archs: dict[str, set[str]] = {}
    smallest_symbol_version: dict[str, str] = read_main_symbols()
    for dist in args.dist:
        latest_symbol_archs: dict[str, set[str]] = {}
        archs = get_archs(dist)
        for deb in download_debs(dist):
            read_symbol_file(deb)
    # Clean up removed symbols
    for symbol in list(symbol_archs):
        if symbol not in latest_symbol_archs:
            del symbol_archs[symbol]
        else:
            symbol_archs[symbol] = latest_symbol_archs[symbol]
    with open(glob.glob("debian/libapt-pkg*.symbols")[0], "w") as sys.stdout:
        print_merged()
