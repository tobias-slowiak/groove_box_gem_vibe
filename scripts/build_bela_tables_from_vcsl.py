#!/usr/bin/env python3
"""
Generate Bela-friendly TSV tables from VCSL SFZ files.

Output layout:
  Samples/bela_tables/vcsl_full/
    README.txt
    instruments.tsv
    samples_needed.tsv
    missing_samples.tsv
    tables/<instrument_id>.zones.tsv
"""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


KEY_PATTERN = re.compile(r"[A-Za-z_][A-Za-z0-9_]*=")
TAG_PATTERN = re.compile(r"<\s*([A-Za-z0-9_]+)\s*>")
NOTE_PATTERN = re.compile(r"^([A-Ga-g])([#b]?)(-?\d+)$")


ZONE_COLUMNS = [
    "zone_id",
    "sample_relpath",
    "lokey",
    "hikey",
    "lovel",
    "hivel",
    "pitch_keycenter",
    "tune_cents",
    "volume_db",
    "pan",
    "trigger",
    "seq_length",
    "seq_position",
    "group",
    "off_by",
    "off_mode",
    "loop_mode",
    "loop_start",
    "loop_end",
    "offset",
    "ampeg_attack",
    "ampeg_decay",
    "ampeg_sustain",
    "ampeg_release",
    "amp_veltrack",
]


def strip_comments(line: str) -> str:
    # SFZ comments are usually // or ; ; both are safe to strip here.
    for token in ("//", ";"):
        idx = line.find(token)
        if idx >= 0:
            line = line[:idx]
    return line.strip()


def parse_note_or_int(value: str) -> int:
    value = value.strip()
    if value == "":
        raise ValueError("Empty note value")
    if re.fullmatch(r"-?\d+", value):
        return int(value)
    m = NOTE_PATTERN.match(value)
    if not m:
        raise ValueError(f"Unsupported note format: {value}")
    note_name, accidental, octave_s = m.groups()
    octave = int(octave_s)
    base = {
        "C": 0,
        "D": 2,
        "E": 4,
        "F": 5,
        "G": 7,
        "A": 9,
        "B": 11,
    }[note_name.upper()]
    if accidental == "#":
        base += 1
    elif accidental == "b":
        base -= 1
    midi = (octave + 1) * 12 + base
    return midi


def clamp_int(value: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, value))


def parse_assignments(segment: str) -> Dict[str, str]:
    """
    Parse "key=value key2=value2" where values may contain spaces.
    Boundary is next `<identifier>=`.
    """
    out: Dict[str, str] = {}
    if not segment:
        return out

    matches = list(KEY_PATTERN.finditer(segment))
    if not matches:
        return out

    for i, m in enumerate(matches):
        key = m.group(0)[:-1].strip().lower()
        start = m.end()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(segment)
        value = segment[start:end].strip()
        if value != "":
            out[key] = value
    return out


def merge_region(global_ops: Dict[str, str], master_ops: Dict[str, str], group_ops: Dict[str, str], region_ops: Dict[str, str]) -> Dict[str, str]:
    merged: Dict[str, str] = {}
    merged.update(global_ops)
    merged.update(master_ops)
    merged.update(group_ops)
    merged.update(region_ops)
    return merged


def normalize_region(ops: Dict[str, str], sfz_dir: Path, vcsl_root: Path) -> Dict[str, str]:
    # Resolve sample path.
    sample_raw = ops.get("sample", "").strip()
    if sample_raw == "":
        return {}
    sample_path = (sfz_dir / sample_raw).resolve()
    try:
        sample_rel = sample_path.relative_to(vcsl_root.resolve()).as_posix()
    except ValueError:
        # Fallback when sample= path is already relative to vcsl root.
        sample_rel = sample_raw.replace("\\", "/").lstrip("./")

    def get_int(key: str, default: int, minv: int | None = None, maxv: int | None = None) -> int:
        raw = ops.get(key, str(default))
        try:
            v = parse_note_or_int(raw)
        except Exception:
            v = default
        if minv is not None and maxv is not None:
            v = clamp_int(v, minv, maxv)
        return v

    def get_float(key: str, default: float) -> float:
        raw = ops.get(key)
        if raw is None:
            return default
        try:
            return float(raw)
        except Exception:
            return default

    key_val = ops.get("key")
    key_midi = None
    if key_val is not None:
        try:
            key_midi = clamp_int(parse_note_or_int(key_val), 0, 127)
        except Exception:
            key_midi = None

    lokey = get_int("lokey", key_midi if key_midi is not None else 0, 0, 127)
    hikey = get_int("hikey", key_midi if key_midi is not None else 127, 0, 127)
    if hikey < lokey:
        hikey = lokey

    pitch_keycenter_default = key_midi if key_midi is not None else lokey
    pitch_keycenter = get_int("pitch_keycenter", pitch_keycenter_default, 0, 127)
    lovel = get_int("lovel", 0, 0, 127)
    hivel = get_int("hivel", 127, 0, 127)
    if hivel < lovel:
        hivel = lovel

    out = {
        "sample_relpath": sample_rel,
        "lokey": str(lokey),
        "hikey": str(hikey),
        "lovel": str(lovel),
        "hivel": str(hivel),
        "pitch_keycenter": str(pitch_keycenter),
        "tune_cents": f"{get_float('tune', 0.0):.6f}",
        "volume_db": f"{get_float('volume', 0.0):.6f}",
        "pan": f"{get_float('pan', 0.0):.6f}",
        "trigger": ops.get("trigger", "attack").strip().lower(),
        "seq_length": str(get_int("seq_length", 1)),
        "seq_position": str(get_int("seq_position", 1)),
        "group": str(get_int("group", 0)),
        "off_by": str(get_int("off_by", 0)),
        "off_mode": ops.get("off_mode", "").strip().lower(),
        "loop_mode": ops.get("loop_mode", "").strip().lower(),
        "loop_start": str(get_int("loop_start", -1)),
        "loop_end": str(get_int("loop_end", -1)),
        "offset": str(get_int("offset", 0)),
        "ampeg_attack": f"{get_float('ampeg_attack', 0.0):.6f}",
        "ampeg_decay": f"{get_float('ampeg_decay', 0.0):.6f}",
        "ampeg_sustain": f"{get_float('ampeg_sustain', 100.0):.6f}",
        "ampeg_release": f"{get_float('ampeg_release', 0.0):.6f}",
        "amp_veltrack": f"{get_float('amp_veltrack', 100.0):.6f}",
    }
    return out


def parse_sfz_regions(sfz_path: Path, vcsl_root: Path) -> List[Dict[str, str]]:
    global_ops: Dict[str, str] = {}
    master_ops: Dict[str, str] = {}
    group_ops: Dict[str, str] = {}
    current_region: Dict[str, str] | None = None
    current_tag: str | None = None
    regions: List[Dict[str, str]] = []

    def flush_region() -> None:
        nonlocal current_region
        if current_region is None:
            return
        merged = merge_region(global_ops, master_ops, group_ops, current_region)
        normalized = normalize_region(merged, sfz_path.parent, vcsl_root)
        if normalized:
            regions.append(normalized)
        current_region = None

    for raw_line in sfz_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = strip_comments(raw_line)
        if not line:
            continue

        # Process all tags found on this line in-order; any tail text after last tag may carry assignments.
        tag_matches = list(TAG_PATTERN.finditer(line))
        cursor = 0
        if tag_matches:
            for i, tm in enumerate(tag_matches):
                # Text before this tag belongs to previous section.
                prefix = line[cursor:tm.start()].strip()
                if prefix:
                    assign = parse_assignments(prefix)
                    if current_tag == "region":
                        if current_region is None:
                            current_region = {}
                        current_region.update(assign)
                    elif current_tag in ("group", "master", "global", "control"):
                        if current_tag == "group":
                            group_ops.update(assign)
                        elif current_tag == "master":
                            master_ops.update(assign)
                        else:
                            global_ops.update(assign)

                tag = tm.group(1).strip().lower()
                if tag == "region":
                    flush_region()
                    current_region = {}
                elif tag == "group":
                    flush_region()
                    group_ops = {}
                elif tag == "master":
                    flush_region()
                    master_ops = {}
                    group_ops = {}
                elif tag in ("global", "control"):
                    flush_region()
                    global_ops = {}
                    master_ops = {}
                    group_ops = {}
                else:
                    # Unknown tag: flush region to keep parsing stable, then treat as neutral.
                    flush_region()
                current_tag = tag
                cursor = tm.end()

            suffix = line[cursor:].strip()
            if suffix:
                assign = parse_assignments(suffix)
                if current_tag == "region":
                    if current_region is None:
                        current_region = {}
                    current_region.update(assign)
                elif current_tag == "group":
                    group_ops.update(assign)
                elif current_tag == "master":
                    master_ops.update(assign)
                elif current_tag in ("global", "control"):
                    global_ops.update(assign)
        else:
            assign = parse_assignments(line)
            if not assign:
                continue
            if current_tag == "region":
                if current_region is None:
                    current_region = {}
                current_region.update(assign)
            elif current_tag == "group":
                group_ops.update(assign)
            elif current_tag == "master":
                master_ops.update(assign)
            else:
                global_ops.update(assign)

    flush_region()
    return regions


def sanitize_id(name: str) -> str:
    out = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
    if out == "":
        out = "instrument"
    return out


def ensure_unique(base: str, used: Dict[str, int]) -> str:
    if base not in used:
        used[base] = 1
        return base
    idx = used[base]
    used[base] = idx + 1
    return f"{base}_{idx+1}"


def write_tsv(path: Path, columns: List[str], rows: Iterable[Dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=columns, delimiter="\t", extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow({k: row.get(k, "") for k in columns})


def main() -> int:
    parser = argparse.ArgumentParser(description="Build Bela-friendly tables from VCSL SFZ files.")
    parser.add_argument(
        "--vcsl-root",
        default="Samples/VCSL-1.2.2-RC",
        help="Path to VCSL root containing .sfz and wav content.",
    )
    parser.add_argument(
        "--out-dir",
        default="Samples/bela_tables/vcsl_full",
        help="Output folder for generated tables.",
    )
    args = parser.parse_args()

    vcsl_root = Path(args.vcsl_root).resolve()
    out_dir = Path(args.out_dir).resolve()
    tables_dir = out_dir / "tables"

    if not vcsl_root.exists():
        raise SystemExit(f"VCSL root does not exist: {vcsl_root}")

    sfz_files = sorted(vcsl_root.rglob("*.sfz"))
    if not sfz_files:
        raise SystemExit(f"No .sfz files found under: {vcsl_root}")

    instrument_rows: List[Dict[str, str]] = []
    sample_rows: List[Dict[str, str]] = []
    missing_rows: List[Dict[str, str]] = []
    unique_sample_paths: set[str] = set()
    used_ids: Dict[str, int] = {}
    total_zones = 0

    for sfz in sfz_files:
        rel_sfz = sfz.relative_to(vcsl_root).as_posix()
        stem_name = sfz.stem
        dir_rel = sfz.parent.relative_to(vcsl_root).as_posix()
        category = "/".join(rel_sfz.split("/")[:2]) if "/" in rel_sfz else ""
        base_id = sanitize_id(rel_sfz[:-4])  # drop ".sfz"
        instrument_id = ensure_unique(base_id, used_ids)

        zones = parse_sfz_regions(sfz, vcsl_root)
        zone_rows: List[Dict[str, str]] = []
        for i, z in enumerate(zones, start=1):
            row = dict(z)
            row["zone_id"] = str(i)
            zone_rows.append(row)

            sample_rel = row["sample_relpath"]
            if sample_rel not in unique_sample_paths:
                unique_sample_paths.add(sample_rel)
                sample_abs = vcsl_root / Path(sample_rel)
                sample_rows.append(
                    {
                        "sample_relpath": sample_rel,
                        "exists": "1" if sample_abs.exists() else "0",
                        "source": "sfz_zone",
                    }
                )
                if not sample_abs.exists():
                    missing_rows.append(
                        {
                            "instrument_id": instrument_id,
                            "sfz_relpath": rel_sfz,
                            "sample_relpath": sample_rel,
                        }
                    )

        total_zones += len(zone_rows)
        write_tsv(tables_dir / f"{instrument_id}.zones.tsv", ZONE_COLUMNS, zone_rows)

        instrument_rows.append(
            {
                "instrument_id": instrument_id,
                "display_name": stem_name,
                "category": category,
                "folder_relpath": dir_rel,
                "sfz_relpath": rel_sfz,
                "zones_file": f"tables/{instrument_id}.zones.tsv",
                "zone_count": str(len(zone_rows)),
            }
        )

    instrument_columns = [
        "instrument_id",
        "display_name",
        "category",
        "folder_relpath",
        "sfz_relpath",
        "zones_file",
        "zone_count",
    ]
    write_tsv(out_dir / "instruments.tsv", instrument_columns, instrument_rows)
    write_tsv(out_dir / "samples_needed.tsv", ["sample_relpath", "exists", "source"], sample_rows)
    write_tsv(out_dir / "missing_samples.tsv", ["instrument_id", "sfz_relpath", "sample_relpath"], missing_rows)

    readme = out_dir / "README.txt"
    readme.write_text(
        "\n".join(
            [
                "Bela-friendly VCSL table export",
                "",
                "Purpose:",
                "- Precomputed instrument/zone tables so Bela runtime does not need SFZ parsing.",
                "",
                "How to copy to SD:",
                "1) Copy this folder as-is to the SD card (recommended destination: /root/Bela/Samples/bela_tables/vcsl_full).",
                "2) Also copy VCSL sample content root so sample_relpath entries resolve (recommended: /root/Bela/Samples/VCSL-1.2.2-RC).",
                "",
                "Files:",
                "- instruments.tsv: one row per SFZ instrument.",
                "- tables/*.zones.tsv: playable zones per instrument (key/velocity/trigger/sample mapping).",
                "- samples_needed.tsv: unique sample files referenced by all zones.",
                "- missing_samples.tsv: references that were missing during generation.",
                "",
                "Field notes:",
                "- sample_relpath is relative to the VCSL content root.",
                "- trigger values are typically attack/release.",
                "- tune_cents and volume_db are direct SFZ values.",
                "",
                "Generator:",
                "- scripts/build_bela_tables_from_vcsl.py",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    print(f"VCSL root: {vcsl_root}")
    print(f"Output: {out_dir}")
    print(f"SFZ instruments: {len(instrument_rows)}")
    print(f"Total zones: {total_zones}")
    print(f"Unique referenced samples: {len(sample_rows)}")
    print(f"Missing sample references: {len(missing_rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
