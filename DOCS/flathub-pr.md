# Flathub submission: PR text

Fork clone: `/home/commander/DCode/other/flathub-repositories/flathub`, branch `add-red`
(from upstream `new-pr`). Open the PR yourself, in the browser; Flathub's policy forbids AI
tools opening submission PRs or writing their commit messages:

    git push origin add-red
    gh pr create --repo flathub/flathub --base new-pr --head keshavbhatt:add-red --title "Add com.ktechpit.red" --web

## Checklist answers

- Please describe the application briefly:
  Red is a YouTube client for the Linux desktop. It shows the YouTube site in a native Qt 6
  window with a smart TV (Leanback) mode driven by keyboard or gamepad, a YouTube Music mode, ad and tracker
  blocking, SponsorBlock, media keys and MPRIS controls, a mini player, and a built-in
  downloader for videos, playlists and audio.
- Video: attach the screen recording of the Flatpak build (`scripts/demo-tour.sh` drives it).
- Flatpak ID follows the rules: yes, `com.ktechpit.red`; ktechpit.com is mine and already
  verified for my other apps.
- Read and followed the requirements and the submission guide: yes.
- I am a developer of the project: yes, the only one.

## Notes for reviewers

- Red is proprietary (`LicenseRef-proprietary=<URL of the LICENSE>` in the metainfo, the
  licence file installed to `/app/share/licenses/com.ktechpit.red`). The build extracts the
  published snap, the same pattern as com.ktechpit.ultimate-media-downloader and
  com.ktechpit.typingmaster, which I also maintain. I am the upstream author, so
  redistribution is granted and extra-data is not needed.
- Development history: Red has been on the Snap Store as `red-app` since 2020 (Red 9), the
  public repository github.com/keshavbhatt/red dates from March 2020, and Red 10 is the
  current release with its changelog there.
- Permissions: `--device=all` is for GPU video decoding and for game controllers in TV mode
  (Chromium reads /dev/input directly; `--device=input` would require Flatpak 1.15.6 on the
  user's side). `--filesystem=/run/udev:ro` is what other Chromium-based apps on Flathub use so
  controllers get enumerated. `--filesystem=xdg-download` is the default download folder; any
  other folder is chosen through the file chooser portal. Notifications and screen inhibit go
  through the portals. `--talk-name=org.kde.StatusNotifierWatcher` is the tray icon,
  `--own-name=org.mpris.MediaPlayer2.red` the media controls.
- The download engine is fetched by the app on first use from its upstream releases into the
  app's data directory and verified by checksum; ffmpeg comes from the runtime.
- Generative AI disclosure (required by the policy): parts of the application code, the
  metainfo text and this manifest were drafted with an AI coding assistant under my direction
  and reviewed by me. <adjust the wording and extent to what you are comfortable stating>
