#!/usr/bin/env python3
"""Fetch and display CDash test details from the Slicer CDash server.

Usage:
    python3 fetch_cdash_test.py <URL_OR_BUILDTESTID> [--output-only] [--json]

Examples:
    python3 fetch_cdash_test.py https://slicer.cdash.org/tests/32245887
    python3 fetch_cdash_test.py 32245887
    python3 fetch_cdash_test.py 32245887 --output-only
    python3 fetch_cdash_test.py 32245887 --json
"""

import json
import re
import sys
import urllib.request
import urllib.error

CDASH_API_URL = "https://slicer.cdash.org/api/v1/testDetails.php"


def extract_buildtestid(arg):
    """Extract numeric buildtestid from a URL or plain number."""
    # Plain numeric ID
    if arg.isdigit():
        return arg
    # URL pattern: .../tests/NNNNN or .../test/NNNNN
    match = re.search(r"/tests?/(\d+)", arg)
    if match:
        return match.group(1)
    return None


def fetch_test_details(buildtestid):
    """Fetch test details JSON from CDash API."""
    url = f"{CDASH_API_URL}?buildtestid={buildtestid}"
    req = urllib.request.Request(url)
    req.add_header("Accept", "application/json")
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode())


def format_test_details(data):
    """Format test details into a readable summary."""
    t = data.get("test", {})
    update = t.get("update", {})

    lines = []
    lines.append(f"Test:     {t.get('test', 'N/A')}")
    lines.append(f"Status:   {t.get('status', 'N/A')}")
    lines.append(f"Details:  {t.get('details', 'N/A')}")
    lines.append(f"Build:    {t.get('build', 'N/A')}")
    lines.append(f"Site:     {t.get('site', 'N/A')}")
    lines.append(f"Time:     {t.get('time', 'N/A')}")
    if update.get("revision"):
        lines.append(f"Revision: {update['revision']}")
    if update.get("revisionurl"):
        lines.append(f"URL:      {update['revisionurl']}")

    # Measurements
    measurements = t.get("measurements", [])
    if measurements:
        lines.append("")
        lines.append("Measurements:")
        for m in measurements:
            lines.append(f"  {m.get('name', '?')}: {m.get('value', '?')}")

    # Command
    command = t.get("command", "")
    if command:
        lines.append("")
        lines.append(f"Command: {command}")

    # Output
    output = t.get("output", "")
    if output:
        lines.append("")
        lines.append("=" * 72)
        lines.append("TEST OUTPUT")
        lines.append("=" * 72)
        lines.append(output)

    return "\n".join(lines)


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        sys.exit(0)

    arg = sys.argv[1]
    output_only = "--output-only" in sys.argv
    raw_json = "--json" in sys.argv

    buildtestid = extract_buildtestid(arg)
    if not buildtestid:
        print(f"Error: Could not extract buildtestid from: {arg}", file=sys.stderr)
        print("Expected a numeric ID or a URL like https://slicer.cdash.org/tests/12345", file=sys.stderr)
        sys.exit(1)

    try:
        data = fetch_test_details(buildtestid)
    except urllib.error.HTTPError as e:
        print(f"Error: HTTP {e.code} fetching buildtestid={buildtestid}", file=sys.stderr)
        if e.code == 400:
            print("The buildtestid may be invalid.", file=sys.stderr)
        sys.exit(1)
    except urllib.error.URLError as e:
        print(f"Error: Could not connect to CDash: {e.reason}", file=sys.stderr)
        sys.exit(1)

    if raw_json:
        print(json.dumps(data, indent=2))
    elif output_only:
        print(data.get("test", {}).get("output", ""))
    else:
        print(format_test_details(data))


if __name__ == "__main__":
    main()
