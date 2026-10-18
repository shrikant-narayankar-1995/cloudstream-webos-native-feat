# Definition of Done

## Absolute compatibility requirement
- [ ] A real, unmodified `.cs3` plugin was used.
- [ ] Original compiled provider code executed on target TV.
- [ ] Original CloudStream API and required dependencies resolved.
- [ ] Original plugin constructor and lifecycle ran.
- [ ] Original provider registered.
- [ ] A real provider operation returned actual results.
- [ ] All execution occurred on TV.
- [ ] No provider source or package was modified.
- [ ] Reproducible logs and commands are stored.
- [ ] TV model, firmware, kit commit, toolchain and plugin hash are recorded.

## App quality
- [ ] Kit tests pass.
- [ ] Host tests pass.
- [ ] Target package builds and verifies.
- [ ] App installs and launches on target.
- [ ] UI supports remote navigation.
- [ ] Plugin failures do not corrupt state.
- [ ] Network errors, cancellation and timeouts are handled.
- [ ] ZIP validation and hash checks are tested.
- [ ] Secrets and cookies are redacted from logs.
- [ ] Licensing and notices are complete.
- [ ] Compatibility matrix distinguishes tested and untested plugins.

If runtime feasibility is blocked, the project is not complete. The accepted deliverable is an evidence-based blocker report, not a false compatibility claim.
