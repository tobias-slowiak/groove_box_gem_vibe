# Acoustic Pop Starter Bank (Open-License, Bela-Ready)

This defines a practical starter bank of common acoustic instruments for pop/rock usage.

## Recommended Sources

| Slot | Source | License | Notes |
| --- | --- | --- | --- |
| Drum kit (acoustic) | FreePats Acoustic Drum Kit (Muldjord downmix) | CC BY 4.0 | Core pop/rock acoustic kit |
| Piano | Salamander Grand Piano | CC BY 3.0 | Main acoustic piano |
| Guitar (steel) | FreePats Acoustic Guitar Steel | GPLv2 with exception + CC0 | Main strummed/plucked acoustic guitar |
| Guitar (nylon) | FreePats Acoustic Guitar Nylon | GPLv2 with exception + CC0 | Softer guitar option |
| Upright bass A | Karoryfer Sneakybass | CC0 | Acoustic/upright bass flavor |
| Upright bass B | Karoryfer Meatbass | CC0 | Alternative acoustic bass flavor |
| Alt drum kit (optional) | Virtuosity Drums | CC0 | Downloaded and tabled; optional FLAC-backed entries only |

## Framework Notes (Important)

- `instrument_sets.txt` already accepts direct `.zones.tsv` paths.
- `drum_sets.txt` now also accepts direct `.zones.tsv` paths in `piece` rows.
- Zone sample lookup now supports these in order:
  1. absolute `sample_relpath`
  2. path relative to the zone table directory
  3. path relative to the zone table pack root (parent of `tables/`)
  4. path relative to `/root/Bela/Samples/VCSL-1.2.2-RC`

This means external SFZ packs can be embedded without merging into VCSL tables.

## Starter Folder Convention

The current integration stores downloaded sources under:

- `Samples/instruments/acoustic_pop_sources/`

Stable alias tables were generated inside each source root (for example `.../tables/aps_*.zones.tsv`).

For mapped drum roles, create split files:

- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_kick.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_rim.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_snare.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_clh.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_oph.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_tom_low.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_tom_mid.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_tom_high.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_crash.zones.tsv`
- `Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_ride.zones.tsv`

## Snippet For `Samples/data/instrument_sets.txt`

```tsv
set	Acoustic Pop Starter
entry	Samples/instruments/acoustic_pop_sources/salamander/SalamanderGrandPianoV3_44.1khz16bit/tables/aps_piano_salamander.zones.tsv	Piano_Salamander
entry	Samples/instruments/acoustic_pop_sources/steel_guitar/FSS-SteelStringGuitar-SFZ-20200521/tables/aps_gtr_steel.zones.tsv	Guitar_Steel
entry	Samples/instruments/acoustic_pop_sources/nylon_guitar/SpanishClassicalGuitar-SFZ-20190618/tables/aps_gtr_nylon.zones.tsv	Guitar_Nylon
entry	Samples/instruments/acoustic_pop_sources/karoryfer_meatbass/tables/aps_bass_upright_a.zones.tsv	Bass_Upright_A
entry	Samples/instruments/acoustic_pop_sources/karoryfer_sneakybass/tables/aps_bass_upright_b.zones.tsv	Bass_Upright_B
```

## Snippet For `Samples/data/drum_sets.txt`

```tsv
set	acoustic_pop_starter_kit	Acoustic Pop Starter Kit
piece	kick	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_kick.zones.tsv
piece	rim	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_rim.zones.tsv
piece	snare	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_snare.zones.tsv
piece	closed_hihat	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_clh.zones.tsv
piece	open_hihat	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_oph.zones.tsv
piece	low_tom	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_tom_low.zones.tsv
piece	mid_tom	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_tom_mid.zones.tsv
piece	high_tom	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_tom_high.zones.tsv
piece	crash	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_crash.zones.tsv
piece	ride	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_ride.zones.tsv
piece	ride_bell	Samples/instruments/acoustic_pop_sources/muldjord/MuldjordKit SFZ+WAV-20201018/tables/aps_drum_ride_bell.zones.tsv
```

## Import Workflow (Host Side Preparation)

1. The packs are already downloaded under `Samples/instruments/acoustic_pop_sources/`.
2. Generate Bela tables for each pack in-place:

```bash
python3 scripts/build_bela_tables_from_vcsl.py \
  --vcsl-root Samples/instruments/acoustic_pop_sources/<pack_root> \
  --out-dir Samples/instruments/acoustic_pop_sources/<pack_root>
```

3. For Muldjord, generate per-role drum zone tables (`aps_drum_*.zones.tsv`) from the full-kit table by filtering `sample_relpath` names.

## Notes On Virtuosity

- `virtuosity_drums` is downloaded and table-generated under `Samples/instruments/acoustic_pop_sources/virtuosity_drums/`.
- Some Virtuosity mappings still reference missing files in this source snapshot.
- The provided optional alias (`aps_virt_snare_center.zones.tsv`) is kept commented in `instrument_sets.txt` because it is FLAC-backed.
