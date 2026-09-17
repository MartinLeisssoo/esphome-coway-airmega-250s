# Host tests

Run from the repository root with a C++17 compiler:

```sh
sh tests/run.sh
```

The tests compile the actual component against small ESPHome stubs. The protocol fixtures contain 96 captured status packets and 32 captured commands. Tests check parsing, exact command bytes, invalid frames, native fan state, power-on sequencing, cancellation, and timeout.

These tests do not simulate electrical UART transport or replace a firmware build:

```sh
esphome config coway-250s.yaml
esphome compile coway-250s.yaml
```

Fixture rows contain only protocol packets, command timestamps, and attribute values. Raw sessions and private diagnostic logs are not included.
