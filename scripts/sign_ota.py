#!/usr/bin/env python3
"""Sign an INKademic firmware image for the device OTA verifier.

The signature is Ed25519 over the raw 32-byte SHA-256 digest of the firmware,
not over the whole image. The release asset must be named <firmware>.bin.sig
and contain the 64-byte raw Ed25519 signature.

The private key is never read from the repository by this script. Generate or
store it in a protected release-secret location, for example:

  python3 scripts/sign_ota.py firmware-x4-pro.bin \
      --key /secure/inkademic-ota-ed25519.pem
"""

import argparse
import hashlib
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--key", required=True, type=Path, help="Ed25519 private key in PEM format")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    firmware = args.firmware.read_bytes()
    digest = hashlib.sha256(firmware).digest()
    output = args.output or Path(str(args.firmware) + ".sig")

    with tempfile.NamedTemporaryFile() as digest_file:
        digest_file.write(digest)
        digest_file.flush()
        signature = subprocess.run(
            ["openssl", "pkeyutl", "-sign", "-rawin", "-inkey", str(args.key), "-in", digest_file.name],
            check=True,
            capture_output=True,
        ).stdout

    if len(signature) != 64:
        raise RuntimeError(f"unexpected Ed25519 signature length: {len(signature)}")
    output.write_bytes(signature)
    print(f"Signed {args.firmware} -> {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
