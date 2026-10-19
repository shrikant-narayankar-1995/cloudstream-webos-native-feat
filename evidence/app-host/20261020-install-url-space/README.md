# Install hangs on a plugin URL with a space (2026-10-20, host)

Report from the TV: a plugin whose repository URL has a space in the file name stayed on
"Installing…". Reproduced on the host with the project's own conformance plugin served by a local
`python3 -m http.server` as `Csweb Conformance.cs3` (no third-party plugin used).

- `1-before-stuck-installing.png`: before the fix. The kit's HttpClient refused the URL ("URL contains
  whitespace") and, as documented, did not call back; the app ignored `send()`'s `false`.
  The server received no request for the file.
- `2-after-installed.png`: after the fix, the URL is sent as OkHttp would (`Csweb%20Conformance.cs3`),
  the server returns 200 and the plugin installs.
- `3-after-refused-url-error.png`: a URL that still cannot be used (space in the host name) now ends
  with "Download failed (network: invalid-request)" and the Install button back.

`log-excerpts.txt`: the matching app log lines (local URLs only).
