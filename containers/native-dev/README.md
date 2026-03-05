# Native dev container

This container pins the laptop-native build/test toolchain so setup issues stay
out of the way.

## Pinned baseline

- Ubuntu `24.04`
- PlatformIO `6.1.18`
- System build tools: `cmake`, `ninja`, `g++`, `pkg-config`

## Usage

Build the image:

```bash
docker compose -f containers/native-dev/compose.yaml build
```

Run the starter bundle inside the container:

```bash
docker compose -f containers/native-dev/compose.yaml run --rm \
  seedbox-native ./scripts/starter_bundle.sh
```

Open an interactive shell:

```bash
docker compose -f containers/native-dev/compose.yaml run --rm \
  seedbox-native
```

The repo is mounted at `/workspace`, so local edits are visible immediately.
