# Task force Vanguard TFAR addon — installer validation

Date: 2026-11-05. Branding build: installer 1.0.1.0, bundled addon R11, plugin 1.0.341-dtr.11.

## Passed

- Windows x64 .NET Framework compilation with C# 5 and warnings treated as errors.
- 130 existing backend assertions, including installing the actual embedded R11 package, verified readback and idempotency.
- Manifest schema/product/revision/hash/size validation; foreign URLs and unsafe redirects rejected; offline newer-version comparisons.
- Unsafe ZIP paths, duplicate/case-colliding entries, reserved names, special files, oversized expansion and truncated/tampered archives rejected.
- Incomplete install and failed state-write recovery preserve the previous release. An older setup cannot downgrade a newer selected release.
- Executable ProductName and FileDescription are exactly `Task force Vanguard TFAR addon`; company remains Hacket2130 and file version is 1.0.1.0.
- All three installer PowerShell scripts parse successfully.
- Branded mod and plugin copies have the approved payload hashes below. Original downloads remain present and unchanged.

| Artifact | Bytes | SHA256 |
|---|---:|---|
| Task-force-Vanguard-TFAR-addon-Setup-R11.exe | 54079488 | AECD3B6C9FAC1CA85BABBB7687FA888B0962CC48B2DC8D7D72BB7AB00372C871 |
| Task-force-Vanguard-TFAR-addon-R11.zip | 54021360 | 711223E98346B869C3F544BB8CE2CF04E9563436EC62C4B1F15482A13F6919AC |
| Task-force-Vanguard-TFAR-addon-R11.ts3_plugin | 4923523 | 31AD957291A969FB29F08910FC67C907B6B3F6069EE505A8131F78C0C962AC69 |
| update.json | 798 | D16E283817DA83DE1E13430C708ACF0450963A843D429C0DF8BCF9F7C7330C5F |

## Scope and pending work

This revision changes setup/public naming. It does not rebuild the embedded TFAR plugin, alter DSP, modify CBA/ACE behavior, install into live Arma/TeamSpeak paths or change the repository/feed identity. The plugin retains its historical Helmet Voice R11 display name; README documents this compatibility boundary.

The repository exists at Hacket2130/helmet-voice-tfar, but the GitHub release and update feed have not been published. No successful live update check/download is claimed. `Check-PublishedRelease.ps1` is ready to verify the real feed and install into a workspace-only test root after publication.

The earlier desktop visual check found DPI clipping, and the UI source was corrected to use content-sized wrapping layouts. The user stopped computer use with Escape before final desktop verification. The branding pass did not resume computer use; final appearance and interactive GUI behavior remain unverified. No TeamSpeak installer or launcher automation was run.

Prior R11 DSP/protocol and package checks remain 115 and 15 respectively. The standard-helmet sample is user-approved; live multiplayer, CBA/ACE operation, performance and Death Trooper intelligibility still require in-game acceptance. Setup is not Authenticode signed. Download SHA256 checks establish integrity against the GitHub feed, not an independent publisher identity.
