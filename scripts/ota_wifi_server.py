#!/usr/bin/env python3
"""Local Wi-Fi OTA manifest and firmware server for INKademic.

This server deliberately implements the small subset of the GitHub Releases
API consumed by OtaUpdater.cpp. It is useful in two situations:

* a diagnostic/rescue build compiled with INKADEMIC_OTA_RELEASE_URL pointing
  at this server; and
* a local catalog/backend used while testing an Unlocker-style Wi-Fi bridge.

It does not pretend to be the vendor bootloader, alter NVS, or write flash
directly. The device still performs the normal OTA validation and A/B slot
switch. Keep the server on a trusted private network.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mimetypes
import os
import socket
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional
from urllib.parse import unquote, urlsplit


DEFAULT_FIRMWARE = ".pio/build/x4-pro/firmware-x4-pro.bin"
DEFAULT_VERSION = "1.7.1-rescue"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_ota_image(path: Path) -> tuple[int, int]:
    """Validate the common ESP app-image header without requiring esptool.

    The server accepts both normal and minimal recovery application images, but
    rejects an empty file and full-flash images whose first byte is not the ESP
    app-image magic. Detailed chip/segment validation remains the device's job.
    """

    size = path.stat().st_size
    if size < 24:
        raise ValueError(f"firmware is too small to be an ESP app image: {size} bytes")
    with path.open("rb") as stream:
        header = stream.read(24)
    if header[0] != 0xE9:
        raise ValueError(
            f"{path} does not start with ESP app-image magic 0xE9; "
            "provide an OTA app .bin, not a full-flash image"
        )
    segment_count = header[1]
    if segment_count == 0 or segment_count > 16:
        raise ValueError(f"invalid ESP app-image segment count: {segment_count}")
    chip_id = int.from_bytes(header[12:14], "little")
    return size, chip_id


def local_ip() -> str:
    """Return the address normally reachable by a Wi-Fi client."""

    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # No packet is sent. This asks the OS which interface it would use.
        probe.connect(("192.0.2.1", 9))
        address = probe.getsockname()[0]
        if address and not address.startswith("127."):
            return address
    except OSError:
        pass
    finally:
        probe.close()
    return "127.0.0.1"


@dataclass(frozen=True)
class Firmware:
    path: Path
    version: str
    device: str
    size: int
    sha256: str
    chip_id: int
    signature_path: Optional[Path]
    notes: str
    channel: str

    @property
    def asset_name(self) -> str:
        return f"firmware-{self.device}.bin"

    @property
    def signature_name(self) -> str:
        return f"{self.asset_name}.sig"


class OtaRequestHandler(BaseHTTPRequestHandler):
    server_version = "INKademic-OTA/1.0"

    @property
    def ota_server(self) -> "OtaServer":
        return self.server  # type: ignore[return-value]

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"[ota] {self.address_string()} {fmt % args}", flush=True)

    def send_json(self, payload: object, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def send_text(self, body: str, status: HTTPStatus = HTTPStatus.OK) -> None:
        data = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def manifest(self) -> dict:
        firmware = self.ota_server.firmware
        base = self.ota_server.base_url
        assets = [
            {
                "name": firmware.asset_name,
                "size": firmware.size,
                "digest": f"sha256:{firmware.sha256}",
                "sha256": firmware.sha256,
                "browser_download_url": f"{base}/assets/{firmware.asset_name}",
            }
        ]
        if firmware.signature_path is not None:
            signature_size = firmware.signature_path.stat().st_size
            if signature_size != 64:
                raise ValueError(f"Ed25519 signature must be 64 bytes, got {signature_size}")
            assets.append(
                {
                    "name": firmware.signature_name,
                    "size": signature_size,
                    "browser_download_url": f"{base}/assets/{firmware.signature_name}",
                }
            )
        return {
            "tag_name": firmware.version,
            "name": f"INKademic {firmware.version} ({firmware.device})",
            "body": firmware.notes,
            "prerelease": firmware.channel != "stable",
            "assets": assets,
        }

    def catalog(self) -> dict:
        firmware = self.ota_server.firmware
        return {
            "schema_version": 1,
            "releases": [
                {
                    "id": f"wifi-rescue-{firmware.version}",
                    "channel": firmware.channel,
                    "name": firmware.version,
                    "version": firmware.version,
                    "released_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
                    "notes": firmware.notes,
                    "firmware_url": f"{self.ota_server.base_url}/assets/{firmware.asset_name}",
                    "firmware_sha256": firmware.sha256,
                    "size": firmware.size,
                    "supported_devices": [firmware.device],
                }
            ],
        }

    def do_GET(self) -> None:
        self.handle_request()

    def do_HEAD(self) -> None:
        self.handle_request()

    def handle_request(self) -> None:
        path = unquote(urlsplit(self.path).path)
        if path in {"/", "/health", "/healthz"}:
            self.send_json(
                {
                    "ok": True,
                    "service": "inkademic-ota",
                    "version": self.ota_server.firmware.version,
                    "device": self.ota_server.firmware.device,
                }
            )
            return

        if path.rstrip("/") == "/api/catalog":
            try:
                self.send_json(self.catalog())
            except ValueError as error:
                self.send_text(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        # Match both the current INKademic endpoint and the old CrossInk path.
        # This lets a correctly configured rescue build use the same server
        # without hard-coding a repository owner into the local tool.
        if path.endswith("/releases/latest"):
            try:
                self.send_json(self.manifest())
            except ValueError as error:
                self.send_text(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        prefix = "/assets/"
        if path.startswith(prefix):
            requested = path[len(prefix) :]
            firmware = self.ota_server.firmware
            if requested == firmware.asset_name:
                self.serve_file(firmware.path)
                return
            if firmware.signature_path is not None and requested == firmware.signature_name:
                self.serve_file(firmware.signature_path)
                return

        self.send_json({"error": "not found"}, HTTPStatus.NOT_FOUND)

    def serve_file(self, path: Path) -> None:
        file_size = path.stat().st_size
        start = 0
        end = file_size - 1
        status = HTTPStatus.OK
        range_header = self.headers.get("Range")
        if range_header and range_header.startswith("bytes="):
            value = range_header[6:].split(",", 1)[0].strip()
            try:
                first, last = value.split("-", 1)
                if first:
                    start = int(first)
                    if last:
                        end = int(last)
                else:
                    suffix = int(last)
                    start = max(0, file_size - suffix)
                if start < 0 or start >= file_size or end < start:
                    raise ValueError
                end = min(end, file_size - 1)
                status = HTTPStatus.PARTIAL_CONTENT
            except ValueError:
                self.send_response(HTTPStatus.REQUESTED_RANGE_NOT_SATISFIABLE)
                self.send_header("Content-Range", f"bytes */{file_size}")
                self.end_headers()
                return

        content_length = end - start + 1
        self.send_response(status)
        self.send_header("Content-Type", mimetypes.guess_type(path.name)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(content_length))
        self.send_header("Accept-Ranges", "bytes")
        if status == HTTPStatus.PARTIAL_CONTENT:
            self.send_header("Content-Range", f"bytes {start}-{end}/{file_size}")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if self.command == "HEAD":
            return

        with path.open("rb") as stream:
            stream.seek(start)
            remaining = content_length
            while remaining:
                chunk = stream.read(min(1024 * 1024, remaining))
                if not chunk:
                    break
                self.wfile.write(chunk)
                remaining -= len(chunk)


class OtaServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, bind: str, port: int, base_url: str, firmware: Firmware):
        super().__init__((bind, port), OtaRequestHandler)
        self.base_url = base_url.rstrip("/")
        self.firmware = firmware


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", type=Path, default=Path(DEFAULT_FIRMWARE), help="OTA app .bin to serve")
    parser.add_argument("--signature", type=Path, help="optional raw 64-byte Ed25519 .sig asset")
    parser.add_argument("--device", default="x4-pro", choices=("x4-pro", "x4"), help="firmware target")
    parser.add_argument(
        "--version",
        default=DEFAULT_VERSION,
        help="manifest version; must be newer than the device version (default: %(default)s)",
    )
    parser.add_argument("--channel", choices=("stable", "beta", "insider"), default="beta")
    parser.add_argument(
        "--notes",
        default=(
            "INKademic Wi-Fi rescue image. Watchdog-safe OTA writer, X4 Pro partition layout, "
            "rollback protection and academic notes/highlights retained."
        ),
    )
    parser.add_argument("--bind", default="0.0.0.0", help="listen address (default: all local interfaces)")
    parser.add_argument("--port", type=int, default=8787, help="listen port (default: %(default)s)")
    parser.add_argument(
        "--base-url",
        help="URL reachable by the reader, e.g. http://192.168.1.23:8787; inferred when omitted",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    firmware_path = args.firmware.expanduser().resolve()
    if not firmware_path.is_file():
        print(f"firmware not found: {firmware_path}", file=sys.stderr)
        return 2

    try:
        size, chip_id = validate_ota_image(firmware_path)
        digest = sha256_file(firmware_path)
    except (OSError, ValueError) as error:
        print(f"invalid firmware: {error}", file=sys.stderr)
        return 2

    signature_path = args.signature.expanduser().resolve() if args.signature else None
    if signature_path is not None:
        if not signature_path.is_file():
            print(f"signature not found: {signature_path}", file=sys.stderr)
            return 2
        if signature_path.stat().st_size != 64:
            print("signature must contain exactly 64 raw Ed25519 bytes", file=sys.stderr)
            return 2

    base_url = args.base_url or f"http://{local_ip()}:{args.port}"
    if not base_url.startswith(("http://", "https://")):
        print("--base-url must start with http:// or https://", file=sys.stderr)
        return 2

    firmware = Firmware(
        path=firmware_path,
        version=args.version,
        device=args.device,
        size=size,
        sha256=digest,
        chip_id=chip_id,
        signature_path=signature_path,
        notes=args.notes,
        channel=args.channel,
    )
    try:
        server = OtaServer(args.bind, args.port, base_url, firmware)
    except OSError as error:
        print(f"could not start OTA server: {error}", file=sys.stderr)
        return 1

    print("INKademic Wi-Fi OTA server")
    print(f"  firmware : {firmware.path}")
    print(f"  target   : {firmware.device} / ESP chip id 0x{firmware.chip_id:04x}")
    print(f"  version  : {firmware.version}")
    print(f"  size     : {firmware.size} bytes")
    print(f"  sha256   : {firmware.sha256}")
    print(f"  manifest : {base_url}/repos/jbfarias/INKademic/releases/latest")
    print(f"  catalog  : {base_url}/api/catalog")
    print(f"  health   : {base_url}/health")
    print("  stop     : Ctrl-C")
    print("WARNING: this server only supplies OTA bytes; the old hard-coded HTTPS")
    print("         CrossInk binary still needs an Unlocker/DNS bridge or a rescue")
    print("         build configured with this manifest URL.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nOTA server stopped")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
