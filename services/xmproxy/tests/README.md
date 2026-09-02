# xmproxy tests

Development-only test rig and regression tests for `xmproxysrv`.
See [../docs/dev-setup.md](../docs/dev-setup.md) for prerequisites and usage.

    ./run-tests.sh            # build, start rig and daemons, run everything, tear down
    ./run-tests.sh --keep     # leave rig and daemons running for manual poking
    ./run-tests.sh --record   # re-record golden replies after an intended change

Nothing in this directory is installed on a device.
