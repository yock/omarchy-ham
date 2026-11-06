#!/usr/bin/env python3
"""Exercise the omarchy-ham CLI against a temporary XDG config directory."""

from __future__ import annotations

import json
import os
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_cli.py <omarchy-ham>", file=sys.stderr)
        return 2

    binary = pathlib.Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory() as temporary:
        environment = os.environ.copy()
        environment["XDG_CONFIG_HOME"] = temporary
        profile = pathlib.Path(temporary) / "omarchy" / "ham" / "station.json"

        def run(*arguments: str, expect: int = 0) -> subprocess.CompletedProcess[str]:
            result = subprocess.run(
                [str(binary), *arguments],
                env=environment,
                capture_output=True,
                text=True,
            )
            if result.returncode != expect:
                raise AssertionError(
                    f"{arguments} exited {result.returncode}, expected {expect}\n"
                    f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
                )
            return result

        assert run("station", "path").stdout.strip() == str(profile)
        assert "no profile" in run("station", "validate").stdout
        assert not profile.exists()

        run("station", "set", "operator.callsign", "n0call")
        stored = json.loads(profile.read_text(encoding="utf-8"))
        assert stored["operator"]["callsign"] == "N0CALL"
        assert run("station", "get", "operator.callsign").stdout.strip() == "N0CALL"
        assert "\n  \"version\"" in profile.read_text(encoding="utf-8")

        run("station", "set", "operator.grid", "fn31pr")
        assert run("station", "get", "operator.grid").stdout.strip() == "FN31pr"

        run("station", "set", "operator.gridSource", "gps")
        assert run("station", "get", "operator.gridSource").stdout.strip() == "gps"
        run("station", "set", "operator.gridSource", "manual")
        assert run("station", "get", "operator.gridSource").stdout.strip() == "manual"

        run("station", "set", "station.paths", "host-audio,tnc")
        assert json.loads(run("station", "get", "station.paths").stdout) == [
            "host-audio",
            "tnc",
        ]

        run("station", "set", "audio.rx.node", "alsa_input.foo")
        run("station", "set", "audio.tx.node", "alsa_output.bar")
        assert run("station", "get", "audio.rx.node").stdout.strip() == "alsa_input.foo"
        assert run("station", "get", "audio.tx.node").stdout.strip() == "alsa_output.bar"

        run("station", "set", "tnc.transport", "serial:/dev/ttyACM0")
        transport = json.loads(run("station", "get", "tnc.transport").stdout)
        assert transport == {"kind": "serial", "path": "/dev/ttyACM0"}
        run("station", "set", "tnc.baud", "38400")
        assert run("station", "get", "tnc.baud").stdout.strip() == "38400"

        run("station", "set", "tnc.transport", "bluetooth:AA:BB:CC:DD:EE:FF")
        transport = json.loads(run("station", "get", "tnc.transport").stdout)
        assert transport == {"kind": "bluetooth", "address": "AA:BB:CC:DD:EE:FF"}

        run("station", "set", "cat.kind", "rigctld")
        assert run("station", "get", "cat.host").stdout.strip() == "localhost"
        assert run("station", "get", "cat.port").stdout.strip() == "4532"
        run("station", "set", "cat.port", "4533")
        assert run("station", "get", "cat.port").stdout.strip() == "4533"

        assert "valid" in run("station", "validate").stdout
        run("station", "migrate")

        before = profile.read_text(encoding="utf-8")
        run("station", "set", "operator.callsign", "x", expect=1)
        assert profile.read_text(encoding="utf-8") == before
        run("station", "set", "station.paths", "both", expect=1)
        run("station", "set", "cat.port", "70000", expect=1)
        run("station", "set", "tnc.transport", "serial:", expect=1)
        run("station", "set", "operator.gridSource", "auto", expect=1)

        run("station", "unset", "audio.tx.node")
        assert run("station", "get", "audio.tx.node").stdout == ""
        run("station", "unset", "tnc")
        assert run("station", "get", "tnc.transport").stdout == ""
        run("station", "unset", "operator")
        assert run("station", "get", "operator.callsign").stdout == ""

        alternate = pathlib.Path(temporary) / "alternate.json"
        run("--profile", str(alternate), "station", "set", "operator.callsign", "K1ABC")
        assert alternate.exists()
        assert (
            run("--profile", str(alternate), "station", "get", "operator.callsign")
            .stdout.strip()
            == "K1ABC"
        )

        profile.write_text("{not json", encoding="utf-8")
        run("station", "validate", expect=1)

        run("bogus", expect=2)
        run("station", "get", "not.a.field", expect=2)
        assert "0.1.0" in run("--version").stdout
        assert "station set" in run("--help").stdout

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
