#!/usr/bin/env bash
# Keep the previous pipeline command available.
exec bash "$(dirname "$0")/../../instrumentation/lift/pipeline.sh" "$@"
