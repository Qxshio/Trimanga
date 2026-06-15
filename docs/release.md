# Release Checklist

1. Update `PROJECT_VERSION` in `CMakeLists.txt`.
2. Update versions in packaging templates.
3. Update `CHANGELOG.md`.
4. Build and test on macOS, Linux, and Windows.
5. Create release archives with CPack.
6. Calculate SHA-256 checksums for release artifacts.
7. Replace placeholder checksums in package templates.
8. Tag the release:

```sh
git tag v0.1.0
git push origin main --tags
```

9. Publish package-manager manifests after the GitHub release is live.
