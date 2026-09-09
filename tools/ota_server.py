#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Serve the release directory over HTTPS for on-device OTA tests.

Example:
    python3 tools/ota_server.py --dir release --port 8443
"""
from __future__ import annotations

import argparse
import http.server
import functools
import ssl
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", default="release", help="directory to serve")
    parser.add_argument("--port", type=int, default=8443)
    parser.add_argument("--cert", default="tools/test-pki/server.crt")
    parser.add_argument("--key", default="tools/test-pki/server.key")
    parser.add_argument("--bind", default="0.0.0.0")
    args = parser.parse_args()

    handler = functools.partial(
        http.server.SimpleHTTPRequestHandler, directory=args.dir)
    httpd = http.server.ThreadingHTTPServer((args.bind, args.port), handler)

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(args.cert, args.key)
    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

    print(f"Serving {args.dir!r} on https://{args.bind}:{args.port} "
          f"(Ctrl+C to stop)")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
