# Phase 2 screen designs (canvas source)

The 16 TV screens (1920×1080) drafted for approval, saved from the design canvas
"CloudStream webOS — screen designs" (claude.ai artifact `Lw84PZyX1Gz3icWjexDBcP`, version
`1792370982-ab4b`, 2026-10-19). Research and open decisions: `../phase2-design-research.md`.

- `canvas.json`: the canvas index (artboard titles, positions, row titles).
- `*.dc.html`: one artboard per screen, in the canvas's Design Component format. They load the
  canvas runtime (`./support.js`, provided by the canvas, not stored here) and Figtree from Google
  Fonts, so they render inside the canvas; opened directly in a browser, the `{{…}}` placeholders
  and `<sc-for>` repeats stay unexpanded.

| File | Screen |
|---|---|
| Main.dc.html | 01 Home |
| Search.dc.html | 02 Search |
| DetailsMovie.dc.html | 03 Details — movie |
| DetailsSeries.dc.html | 04 Details — series & episodes |
| Sources.dc.html | 05 Source picker |
| Player.dc.html | 06 Player — controls |
| PlayerTracks.dc.html | 07 Player — audio, subtitles, quality |
| PlayerNext.dc.html | 08 Player — skip intro, episodes, up next |
| Library.dc.html | 09 Library |
| Downloads.dc.html | 10 Downloads |
| Extensions.dc.html | 11 Extensions — repositories & plugins |
| PluginSettings.dc.html | 12 Plugin settings (rendered from the plugin) |
| Profiles.dc.html | 13 Profiles |
| Setup.dc.html | 14 First-run setup |
| Settings.dc.html | 15 Settings |
| Accounts.dc.html | 16 Accounts & sync (QR sign-in) |

Artwork is placeholder colour blocks; titles are public-domain works or invented examples.
