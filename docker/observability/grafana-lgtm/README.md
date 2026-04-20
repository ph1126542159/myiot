# Grafana LGTM

This stack uses Grafana's official `grafana/otel-lgtm` image to provide a single-box OpenTelemetry backend for development, demo, and test environments.

It exposes:

- `3000` for the Grafana web UI
- `4317` for OTLP/gRPC ingestion
- `4318` for OTLP/HTTP ingestion

Usage:

```bash
cp .env.example .env
mkdir -p data
docker compose up -d
```

Default web login:

- user: `admin`
- password: `admin`

The application can keep exporting telemetry to:

- `http://<server>:4318`
