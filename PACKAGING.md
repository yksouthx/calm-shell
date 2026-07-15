# Packaging notes (maintainer-only)

Two AUR packages are provided under `packaging/aur/`:

- **`calm-shell`** — builds from a tagged release tarball (`vX.Y.Z`)
- **`calm-shell-git`** — builds from the latest commit on `master`

AUR packages live in their own tiny git repos on `aur.archlinux.org`
(one repo per pkgname, containing only `PKGBUILD` + `.SRCINFO`) — they
are *not* part of this repo's git history. The files under
`packaging/aur/` here are the source of truth; push copies of them to
the AUR repos when publishing.

## Cutting a release (for the `calm-shell` package)

1. Bump `version` in `Cargo.toml`.
2. `cargo build --release --locked` and commit the updated `Cargo.lock`.
3. Tag and push: `git tag v0.1.0 && git push origin v0.1.0`
4. In `packaging/aur/calm-shell/PKGBUILD`, bump `pkgver` (and reset
   `pkgrel=1`), then regenerate the checksum:
   ```bash
   updpkgsums   # from pacman-contrib, fills in sha256sums
   makepkg --printsrcinfo > .SRCINFO
   ```
5. Test the build in a clean chroot or container:
   ```bash
   makepkg -si
   ```
6. Push `PKGBUILD` + `.SRCINFO` to the `calm-shell` AUR repo:
   ```bash
   git clone ssh://aur@aur.archlinux.org/calm-shell.git aur-calm-shell
   cp packaging/aur/calm-shell/{PKGBUILD,.SRCINFO} aur-calm-shell/
   cd aur-calm-shell && git add -A && git commit -m "release v0.1.0" && git push
   ```

## Updating `calm-shell-git`

This package always builds HEAD, so nothing needs bumping in the
PKGBUILD itself — `pkgver()` computes the version from `git rev-list`
at build time. Only touch it if `pkgrel`, dependencies, or the install
step change. Regenerate `.SRCINFO` after any edit and push the same
way as above, to the `calm-shell-git` AUR repo.

## Local testing without an AUR push

```bash
cd packaging/aur/calm-shell     # or calm-shell-git
makepkg -si                     # build + install locally
calm doctor                     # sanity check
```
