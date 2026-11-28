# Third-party notices

Original TKGPU code is licensed under [MIT](LICENSE). The following bundled
components retain their upstream terms; the project license does not replace
them.

| Component | Included material | License and notices |
| --- | --- | --- |
| zlib 1.3.1 | Unmodified compression source subset built with `Z_SOLO` | [License](third_party/zlib/LICENSE), [inclusion notice](third_party/zlib/TKGPU-NOTICE.txt) |
| Unicode Character Database 16.0.0 | Generated character-category and simple case-mapping tables | [Unicode License V3](third_party/unicode/LICENSE.txt), [provenance and scope](third_party/unicode/README.md) |

Keep the applicable full licenses and notices with redistributions containing
these materials. The development host bundler includes copies in its inventory.

Python, X11, Pango/Cairo, platform runtimes and optional fonts may add dependencies
or redistribution obligations to a particular build. They are not all bundled
by this repository. See the [README dependency table](README.md#external--optional-system--development-dependencies-not-bundled)
and [distribution review notes](docs/legal/DEVELOPMENT_AND_DISTRIBUTION.md).
