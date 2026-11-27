# TKGPU development and distribution review

This is an engineering policy and due-diligence checklist, not a new license,
a copyright ownership declaration or permission to distribute. The repository
LICENSE and actual third-party terms continue to apply. Developer bundles are
not approved commercial releases.

## Independence and provenance

- Implement host ABI and compatibility behavior using original project code and
  public API specifications. Do not import third-party implementation source
  without recording its exact version, origin, license and applicable notices.
- Retain all existing copyright notices. Do not relabel external code as Kerbot
  code. Review historical contributions and earlier grants before publication.
- Tkinter compatibility is an interoperability claim, not PSF or Tcl/Tk
  endorsement. Compatibility version labels are not the TKGPU ABI version.
- Do not publish a private repository, its history, source, assets or a binary
  SDK without the separate rights-holder and release review.

## Distribution inventory (review exact versions)

| Component | Role | Distribution consideration |
| --- | --- | --- |
| CPython / tkinter | External Python API/runtime | PSF and associated notices; include full applicable license and a change summary if distributing modified Python |
| Tcl/Tk | Optional reference tests and old staged bridge | Retain upstream notices and full applicable terms for any distributed portions; direct host does not require the original Tcl/Tk binaries |
| Pillow and image codecs | Optional external image decoding | Review Pillow and actual codec/binary dependencies; preserve notices |
| X11 | Linux native window backend | Review exact libraries and permissive notices |
| Pango and dependencies | Optional Linux shaping | LGPL-2.1-or-later for Pango; verify exact dependencies, linking, replacement/relinking and source/notice obligations for the actual distribution |
| Windows SDK/CRT and compiler runtimes | Native platform/toolchain | Verify redistribution rights and runtime licenses for each build |

Official references:
- https://docs.python.org/3/license.html
- https://www.tcl-lang.org/software/tcltk/license.html
- https://gnome.pages.gitlab.gnome.org/pango/Pango/

The independent development bundle contains a native TKGPU binary, the explicitly
listed project-owned binding files (subject to provenance review), its public
host header and the repository license and this notice. It does not deliberately
copy stock Python/Tcl/Tk, Pillow, Pango, fonts or compiler runtimes. A binary
may still contain or link third-party material; absence of separately copied
libraries does not establish license compliance.

## Release gate (manual, mandatory)

Record the exact source revision, contributors and licensing history; generate
the real dependency/SBOM and inspect binary imports and static linkage per OS;
include full applicable notices, source/source-offer and relinking arrangements
where required; review branding and ownership; authorize the distribution.
Do not call a development manifest a legal audit or claim that the original
code is verified clean-room. No rights in third-party software are restricted
by this checklist or by proprietary terms for original project code.
