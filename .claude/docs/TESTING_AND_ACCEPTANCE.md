# Testing and Acceptance

## Host checks
Run the kit's documented doctor, test and build commands. Add unit tests for archive parsing, manifests and hashes as applicable. Run sanitizers where supported.

## Build/package
Build with the kit workflow. Verify the generated IPK using its verification script and documented runtime-libraries configuration. Check contents, architecture, permissions and license notices. Do not bypass checks by hand-editing packages.

## Device checks
Install and launch an unmodified kit example. Record TV model, firmware, Developer Mode state, kit commit, NDK version and package hash. Gather logs with kit tools and run device tests if supported.

## Plugin integration
Use a real unmodified plugin. Record source, hash, manifest, entry class and API versions. Verify original entry class load, lifecycle, provider registration and one real provider operation. Prove execution is local. Test unload/reload and failures.

## Security
Test ZIP traversal, oversized archives, malformed manifests, missing DEX, bad hashes, TLS failures, redirects/cookies, log redaction and recovery from plugin crashes.

## Acceptance
No compatibility claim without an unmodified real plugin passing on the target TV. Track each plugin as PASS, PARTIAL, FAIL, or UNTESTED with evidence.
