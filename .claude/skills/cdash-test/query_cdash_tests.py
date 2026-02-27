#!/usr/bin/env python3
"""Query CDash for recent results of a test across all sites/builds.

Usage:
    python3 query_cdash_tests.py <test_name> [--days N] [--project PROJECT] [--site SITE] [--failed-only]

Examples:
    python3 query_cdash_tests.py qSlicerSslTest
    python3 query_cdash_tests.py qSlicerSslTest --days 14
    python3 query_cdash_tests.py qSlicerSslTest --site metroplex.kitware
    python3 query_cdash_tests.py qSlicerSslTest --failed-only
"""

import json
import sys
import urllib.request
import urllib.error
import urllib.parse

CDASH_BASE = "https://slicer.cdash.org"
# CDash project names to try in order (the public API is picky about exact names)
PROJECT_NAMES = ["SlicerPreview", "Slicer", "SlicerStable"]

DEFAULT_DAYS = 7


def query_tests(test_name, *, project=None, days=DEFAULT_DAYS, site=None, failed_only=False):
    """Query CDash queryTests API for recent results of a given test."""
    projects_to_try = [project] if project else PROJECT_NAMES

    for proj in projects_to_try:
        params = {
            "project": proj,
            "filtercombine": "and",
            "limit": "50",
        }

        # Build filters
        filter_idx = 1

        # Test name filter (exact match = compare 61)
        params[f"field{filter_idx}"] = "testname"
        params[f"compare{filter_idx}"] = "61"
        params[f"value{filter_idx}"] = test_name
        filter_idx += 1

        # Date range filter
        if days:
            params[f"field{filter_idx}"] = "buildstarttime"
            params[f"compare{filter_idx}"] = "83"  # is after
            params[f"value{filter_idx}"] = f"{days} days ago"
            filter_idx += 1

        # Site filter
        if site:
            params[f"field{filter_idx}"] = "site"
            params[f"compare{filter_idx}"] = "61"
            params[f"value{filter_idx}"] = site
            filter_idx += 1

        params["filtercount"] = str(filter_idx - 1)

        url = f"{CDASH_BASE}/api/v1/queryTests.php?{urllib.parse.urlencode(params)}"
        req = urllib.request.Request(url)
        req.add_header("Accept", "application/json")

        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                data = json.loads(resp.read().decode())
        except (urllib.error.HTTPError, urllib.error.URLError):
            continue

        if "error" in data:
            continue

        builds = data.get("builds", [])
        if builds or project:
            # Found results or user specified a project explicitly
            return proj, builds

    return None, []


def format_results(project, builds, failed_only=False):
    """Format query results into a readable table."""
    if not builds:
        return "No results found."

    if failed_only:
        builds = [b for b in builds if b.get("status") == "Failed"]
        if not builds:
            return "No failures found in the given time range."

    lines = []
    lines.append(f"Project: {project}  |  Results: {len(builds)}")

    # Count pass/fail
    passed = sum(1 for b in builds if b.get("status") == "Passed")
    failed = sum(1 for b in builds if b.get("status") == "Failed")
    lines.append(f"Passed: {passed}  |  Failed: {failed}")
    lines.append("")
    lines.append(f"{'Status':<10} {'Date':<12} {'Time':>8}  {'Site':<25} {'Build'}")
    lines.append("-" * 95)

    for b in builds:
        status = b.get("status", "?")
        marker = ">>>" if status == "Failed" else "   "
        date = b.get("buildstarttime", "?")[:10]
        time_s = b.get("time", "?")
        site = b.get("site", "?")
        build = b.get("buildName", "")
        # Truncate build name for readability
        if len(build) > 40:
            build = build[:37] + "..."
        lines.append(f"{marker} {status:<10} {date:<12} {time_s:>8}s  {site:<25} {build}")

    return "\n".join(lines)


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        sys.exit(0)

    test_name = sys.argv[1]
    days = DEFAULT_DAYS
    project = None
    site = None
    failed_only = False

    # Simple arg parsing
    args = sys.argv[2:]
    i = 0
    while i < len(args):
        if args[i] == "--days" and i + 1 < len(args):
            days = int(args[i + 1])
            i += 2
        elif args[i] == "--project" and i + 1 < len(args):
            project = args[i + 1]
            i += 2
        elif args[i] == "--site" and i + 1 < len(args):
            site = args[i + 1]
            i += 2
        elif args[i] == "--failed-only":
            failed_only = True
            i += 1
        elif args[i] == "--json":
            # Will be handled after query
            i += 1
        else:
            print(f"Unknown argument: {args[i]}", file=sys.stderr)
            sys.exit(1)

    proj, builds = query_tests(test_name, project=project, days=days, site=site, failed_only=failed_only)

    if "--json" in sys.argv:
        print(json.dumps({"project": proj, "builds": builds}, indent=2))
    else:
        print(format_results(proj, builds, failed_only=failed_only))


if __name__ == "__main__":
    main()
