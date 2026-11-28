# Unicode-derived data

TKGPU includes tables derived from **Unicode Character Database 16.0.0**:

- `src/compat/TKGPUUnicodeClasses.H`: general-category classification ranges.
- `src/compat/TKGPUUnicodeCase.H`: simple lowercase, uppercase and titlecase maps.

These are generated data tables, not Tcl implementation source. Their Unicode
data is covered by the [Unicode License V3](LICENSE.txt). Preserve the full
copyright and permission notice when redistributing the tables or software
containing them, including in binary form.

Upstream references:

- Database: <https://www.unicode.org/Public/16.0.0/ucd/>
- Version description: <https://www.unicode.org/Public/16.0.0/ucd/ReadMe.txt>
- License: <https://www.unicode.org/license.txt>

`LICENSE.txt` is a verbatim copy of the upstream license retrieved on
2026-11-28. The generated table headers identify data version 16.0; the current
license notice's copyright year does not change that data version.

This notice does not cover third-party font designs, bitmap glyph packs or
optional text-rendering libraries. Check their own licenses if supplied with a
build. Unicode and the Unicode Logo are registered trademarks of Unicode, Inc.;
no endorsement is implied.
