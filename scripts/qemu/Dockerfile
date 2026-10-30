# QEMU (PowerPC mac99) plus hfsutils, for scripts/test-qemu-os9.sh.
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
        qemu-system-ppc qemu-system-data seabios qemu-utils hfsutils python3 netpbm \
    && rm -rf /var/lib/apt/lists/*
