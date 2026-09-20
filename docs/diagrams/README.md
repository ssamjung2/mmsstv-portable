# Diagrams

Visual documentation, kept as **source** rather than exported pictures, so it
can be reviewed in a pull request and edited years later.

| File | Shows | Referenced by |
| --- | --- | --- |
| [architecture.drawio](architecture.drawio) | Front ends, the API, the core modules and what they depend on | [application-architecture.md](../plans/application-architecture.md) |
| [deployment.drawio](deployment.drawio) | How the pieces are deployed on Pi, Linux desktop and Apple platforms | [application-architecture.md](../plans/application-architecture.md) |
| [wireframes.drawio](wireframes.drawio) | Monitor and Compose at desktop, Pi touchscreen and phone sizes | [ui-ux-design.md](../plans/ui-ux-design.md) |

## Two formats, deliberately

- **draw.io (`.drawio`)** for diagrams with real layout: architecture,
  deployment, wireframes. The files here are uncompressed XML, so a diff shows
  what changed instead of one opaque blob. Edit them with
  [draw.io Desktop](https://github.com/jgraph/drawio-desktop) (Apache 2.0,
  offline) or diagrams.net in a browser. Do not switch the file to the
  compressed format: in draw.io, **File → Properties → Compressed: off**.
- **Mermaid**, written inline in the Markdown, for flows and state machines
  that are mostly boxes and arrows. It renders on GitHub and in most editors,
  needs no tool to edit, and stays readable as plain text. The QSO flow in
  [ui-ux-design.md](../plans/ui-ux-design.md) and the layer diagram in
  [application-architecture.md](../plans/application-architecture.md) are
  Mermaid.

The rule of thumb: if hand-placing the boxes carries meaning, use draw.io; if
the graph explains itself, use Mermaid.

## Keeping them honest

- A diagram that contradicts the code is worse than no diagram. Update it in
  the same pull request as the change it describes.
- Every diagram is referenced from a document. An unreferenced diagram is dead
  and should be deleted.
- No screenshots of diagrams in the documentation; link the source file so the
  reader can open and edit it.
- Exported PNG or SVG copies are fine for release notes and the website, but
  they are build artefacts and are not committed here.
