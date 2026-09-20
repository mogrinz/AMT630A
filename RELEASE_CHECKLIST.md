# Release checklist: AMT630A OSD 0.7.0

Before creating the public GitHub tag/release:

1. Create the GitHub repository with `library.properties` at the repository root.
2. Confirm `url=https://github.com/mogrinz/AMT630A` in `library.properties`.
3. MIT license selected and `LICENSE` added. Confirm copyright holder/year before publishing. `library.properties` declares `license=MIT`.
4. Run Arduino Lint in Library Manager submission mode, or push to GitHub and confirm the bundled `Arduino Lint` workflow passes.
5. Test at least the basic text examples and the hardware-specific examples you intend to advertise on the target ESP32/AMT630A board.
6. Commit the release contents.
7. Create and push a semantic-version Git tag such as `0.7.0` or `v0.7.0`. The `library.properties` version must remain `0.7.0` for that tag.
8. Create a GitHub Release from that tag (recommended, though the tag itself is what the Arduino indexer detects).
9. For the first Arduino Library Manager publication, submit the repository URL to the Arduino `library-registry` repository according to its current submission instructions.
10. After acceptance, future versions only require a new `library.properties` version plus a new compliant Git tag/release; the Arduino indexer checks registered repositories automatically.

Do not add a `.development` file or symlinks to a Library Manager release repository.