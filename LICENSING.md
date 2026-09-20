# Licensing

This repository holds code under two licences. The split follows the module
boundary, and [ADR-0015](docs/decisions/0015-licensing-of-new-components.md)
explains why.

| Part | Licence | Why |
| --- | --- | --- |
| `src/`, `include/` — the SSTV encoder and decoder | **LGPL v3** | Derived from MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba. Not a choice: the licence came with the code |
| `core/`, `apps/`, `tests/test_station_api.cpp`, `tests/fuzz_station_api.cpp` — PocketSSTV | **Apache-2.0** | New work. Patent grant, App Store compatible, and others may embed it |
| `external/` | Their own | `stb_image` is public domain / MIT, as stated in the files |

Every source file carries an `SPDX-License-Identifier` so this is verifiable
by tooling rather than by reading prose.

## What this means if you distribute a binary

The LGPL parts must stay **dynamically linked and relinkable**: ship them as a
shared library or framework, and provide what a user needs to rebuild them and
relink your application. This constraint shapes the Apple build in particular
([ADR-0006](docs/decisions/0006-licensing-and-app-store.md)).

Mixing the two is checked in CI: an LGPL object compiled into an Apache-2.0
binary is a defect, not a nuance.

## Licence texts

- `LICENSE` — the LGPL v3 text, as inherited from MMSSTV.
- `LICENSE-APACHE-2.0` — **not yet in the repository.** Add the canonical text
  rather than a transcription:

  ```sh
  curl -o LICENSE-APACHE-2.0 https://www.apache.org/licenses/LICENSE-2.0.txt
  ```

## Third-party material

The SSTV Handbook (*Image Communication on Short Waves* by Martin Bruchanov,
OK2MNM, <https://www.sstv-handbook.com>) is **cited, not redistributed**. It is
gitignored, and was removed from this repository's history. Download your own
copy from the author's site.
