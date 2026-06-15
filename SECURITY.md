# Security Policy

Trimanga processes local manga archives and image folders. Treat untrusted archives carefully, especially when extracting files from unknown sources.

## Supported Versions

Security fixes target the latest released version.

## Reporting a Vulnerability

Open a private security advisory on GitHub for `Qxshio/Trimanga` if available, or contact the maintainer directly.

Please include:

- A short description of the issue.
- Steps to reproduce.
- Platform and version information.
- Whether the issue involves archive extraction, OCR execution, path traversal, or unsafe deletion.

## Safety Expectations

Trimanga should not delete pages by default. Any future destructive cleanup mode must require explicit user intent and should keep enough state for review or recovery.
