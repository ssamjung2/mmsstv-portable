# Data model

Everything the station stores: on disk, in the database, in configuration, and
in a template document. [ADR-0008](../decisions/0008-storage.md) chose the
shapes; this specifies them.

The schema below is a starting point to implement against, not a finished
migration. It will change during M1 — but it will change from *this*, with a
migration, rather than being invented three times in three modules.

## Identifiers

| Kind | Form | Why |
| --- | --- | --- |
| Picture | `p_` + ULID | Sortable by creation time, no coordination needed |
| Contact | `q_` + ULID | |
| Template | `t_` + slug | Human-chosen, appears in scripts |
| Content hash | SHA-256 of the pixel file | Deduplication, integrity |
| Device | Stable platform id + name hash | Survives replugging and reordering (`R-AUD-7`) |

ULIDs, not auto-increment integers: a station that is restored from backup or
merged from two machines must not collide.

## On disk

```text
$XDG_DATA_HOME/pocketsstv/            (~/Library/Application Support/PocketSSTV, app container on iOS)
  station.db                    SQLite: library, log, settings that are not files
  pictures/
    ab/cd/abcdef…​.png           content-addressed, two-level fan-out
  work/
    p_01H….partial              lines written as they decode; recovered at startup
  templates/
    t_reply.json                template documents
    assets/                     images used by templates
  exports/                      ADIF and picture exports the operator asked for
$XDG_CONFIG_HOME/pocketsstv/config.toml
$XDG_STATE_HOME/pocketsstv/           logs, diagnostics bundles
```

Pictures are ordinary files an operator can back up, sync or point another
program at. The database can always be rebuilt from them (`library.repair`).

## Database

SQLite, WAL mode, `foreign_keys=ON`, integrity-checked at startup.

```sql
CREATE TABLE picture (
  id            TEXT PRIMARY KEY,           -- p_ULID
  direction     TEXT NOT NULL,              -- 'rx' | 'tx'
  status        TEXT NOT NULL,              -- 'complete' | 'partial' | 'aborted'
  content_hash  TEXT NOT NULL,              -- SHA-256; file path derives from this
  format        TEXT NOT NULL,              -- 'png' | 'jpeg'
  width         INTEGER NOT NULL,
  height        INTEGER NOT NULL,
  mode          TEXT NOT NULL,              -- 'Martin 1'
  mode_enum     INTEGER NOT NULL,           -- sstv_mode_t, for the libraries
  started_utc   TEXT NOT NULL,              -- ISO-8601 UTC
  ended_utc     TEXT,
  clock_quality TEXT NOT NULL,              -- 'synchronised' | 'unsynchronised' | 'corrected'
  frequency_hz  INTEGER,                    -- NULL when no rig was connected
  rig_mode      TEXT,                       -- 'USB', 'LSB', …
  callsign      TEXT,                       -- operator-assigned or parsed
  contact_id    TEXT REFERENCES contact(id) ON DELETE SET NULL,
  lines_total   INTEGER NOT NULL,
  lines_decoded INTEGER NOT NULL,
  quality       TEXT,                       -- JSON: see below
  audio_path    TEXT,                       -- optional recording (R-RX-10)
  notes         TEXT
);
CREATE INDEX picture_time  ON picture(started_utc DESC);
CREATE INDEX picture_call  ON picture(callsign);
CREATE INDEX picture_mode  ON picture(mode);

CREATE TABLE contact (                      -- one QSO
  id            TEXT PRIMARY KEY,
  callsign      TEXT NOT NULL,
  started_utc   TEXT NOT NULL,
  ended_utc     TEXT,
  clock_quality TEXT NOT NULL,
  band          TEXT,                       -- '20m'
  frequency_hz  INTEGER,
  rig_mode      TEXT,
  sstv_mode     TEXT,
  rsv_sent      TEXT,                       -- SSTV reports are RSV, not RST
  rsv_received  TEXT,
  name          TEXT,
  qth           TEXT,
  grid          TEXT,
  comment       TEXT,
  adif_extra    TEXT                        -- JSON: fields we round-trip but do not model
);
CREATE INDEX contact_time ON contact(started_utc DESC);
CREATE INDEX contact_call ON contact(callsign);

CREATE TABLE schema_version (version INTEGER NOT NULL);
```

`quality` holds what the decoder measured, as JSON, so it can grow without a
migration:

```json
{"signal_peak": 0.62, "snr_estimate_db": 18.4, "sync_locked_fraction": 0.98,
 "timing_error_samples": 1.7, "timing_correction_ppm": 43.0,
 "dropped_lines": 2, "agc_gain": 12.5}
```

### Migrations

- `schema_version` is checked at startup. Newer than the binary understands →
  refuse to write, open read-only, tell the operator to upgrade.
- Migrations are forward-only, numbered, idempotent, and each ships with a
  test that runs it against a fixture database from the previous release
  ([test strategy](test-strategy.md#upgrade-and-migration)).
- The database is backed up to `station.db.pre-<n>` before a migration runs.

## Time

Timestamps are ISO-8601 UTC strings with a companion `clock_quality`, because
a Raspberry Pi with no real-time clock and no network boots in 1970:

- `synchronised` — the system clock was trustworthy when written.
- `unsynchronised` — written before time was available; relative ordering is
  still correct, absolute values are not.
- `corrected` — was `unsynchronised`, later shifted by a measured offset.

At startup the daemon records monotonic and wall-clock time; when a step
larger than 60 seconds is observed, it corrects `unsynchronised` rows and
marks them. ADIF export refuses unsynchronised records unless forced
(`R-TIME-1`).

## Configuration

One TOML file, every key documented, `config.describe` serving the same text
to settings screens so no front end hard-codes the list.

```toml
[station]
callsign = "M0XYZ"
grid = "IO91wm"
operator = "Alex"

[audio]
input = "usb-codec-1:capture"      # stable device id
output = "usb-codec-1:playback"
input_gain_db = 0.0
rx_clock_ppm = 0.0                 # measured by audio.calibrate
tx_clock_ppm = 0.0
target_peak_dbfs = -6.0            # drive calibration target (R-TX-10)

[ptt]
method = "serial-rts"              # none | vox | serial-rts | serial-dtr | cat | cm108 | gpio
port = "/dev/ttyUSB0"
gpio_chip = "gpiochip0"; gpio_line = 17
lead_in_ms = 100
tail_ms = 50
max_transmit_s = 0                 # 0 = derive from the mode, plus margin

[rig]
backend = "hamlib"                 # none | hamlib | rigctld
model = 2                          # hamlib model number
port = "/dev/ttyUSB1"; baud = 38400
host = ""                          # for rigctld
poll_interval_ms = 1000

[rx]
auto_start = true
trigger_sensitivity = "medium"     # lowest | low | medium | high
auto_stop = true
sync_lost_timeout_s = 5.0
auto_slant = true
auto_save = true
save_format = "png"                # png | jpeg
jpeg_quality = 90

[tx]
default_mode = "Martin 1"
follow_received_mode = true
cwid_enabled = false
cwid_text = "DE %m"
cwid_wpm = 20
tune_frequency_hz = 1750

[library]
retention_days = 0                 # 0 = keep everything
max_bytes = 0                      # 0 = unlimited
reserve_bytes = 209715200          # stop auto-save below this free space

[network]
enabled = false
bind = "127.0.0.1"; port = 4544
mdns = false

[ui]
theme = "system"                   # system | dark | light
locale = ""                        # empty = system
```

Rules: unknown keys are preserved and warned about, never silently dropped;
the file is rewritten atomically; secrets (tokens) live in
`$XDG_STATE_HOME`, not here.

## Template documents

A template is JSON: a stack of layers over the picture, rendered in the core
so every platform produces identical pixels
([ADR-0007](../decisions/0007-template-rendering.md)).

```json
{
  "id": "t_reply", "name": "Reply card", "version": 1,
  "canvas": {"reference": "mode", "safe_area": {"top": 16}},
  "layers": [
    {"type": "rect", "x": 0, "y": 0, "w": 320, "h": 24,
     "fill": "#000000c0"},
    {"type": "text", "x": 8, "y": 4, "w": 304, "h": 18,
     "text": "{their_call} de {my_call}  {rsv}",
     "font": {"family": "Inter", "size_px": 16, "weight": 600},
     "fill": "#ffffff", "align": "center",
     "stroke": {"color": "#000000", "width_px": 2},
     "overflow": "shrink"},
    {"type": "image", "src": "assets/logo.png",
     "x": 250, "y": 200, "w": 60, "h": 48, "opacity": 0.9},
    {"type": "colorbar", "x": 0, "y": 0, "w": 320, "h": 16, "style": "smpte"}
  ]
}
```

- **Fields** in `{braces}` bind to the QSO context: `their_call`, `my_call`,
  `rsv`, `date_utc`, `time_utc`, `band`, `frequency`, `mode`, `grid`, `name`,
  `free1`…`free4`. An unbound field renders as empty, never as `{their_call}`.
- Coordinates are in the mode's pixel space; `canvas.reference` makes a
  template reusable across modes with different geometry.
- `overflow` decides what a long callsign does: `shrink`, `clip` or `wrap`.
  This must be specified, because "G0ABC/P working portable" overflowing a
  card is the common case, not the edge case.
- `safe_area.top` is the 16-line header band (`R-IMG-9`).
- Colours are `#rrggbb` or `#rrggbbaa`: real alpha, not colour keying.
- Fonts are resolved from a bundled set first, so a template looks the same on
  a Pi with no fonts installed as it does on a Mac. Missing glyphs fall back
  through a CJK-capable font.

Version `1` is the only version; the renderer refuses unknown versions rather
than guessing.

## What ADIF maps to

| ADIF | Our field | Note |
| --- | --- | --- |
| `CALL` | `contact.callsign` | |
| `QSO_DATE`, `TIME_ON` | `started_utc` | UTC always |
| `BAND`, `FREQ` | `band`, `frequency_hz` | Derived from the rig when present |
| `MODE` | `"SSTV"` | ADIF has no per-SSTV-mode field |
| `SUBMODE` | `sstv_mode` | Where the importer tolerates it |
| `RST_SENT`, `RST_RCVD` | `rsv_sent`, `rsv_received` | SSTV reports RSV; we carry the string through unchanged |
| `GRIDSQUARE`, `NAME`, `QTH`, `COMMENT` | same | |
| everything else | `adif_extra` | Round-tripped, never discarded |

Round-tripping is a tested requirement: import then export must preserve every
field a logger sent us ([test strategy](test-strategy.md#interoperability)).
