#!/bin/bash

CURRENT_DIR=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

curl -X POST -H "Content-Type: application/json" \
  http://localhost:8080 \
  --data @"$CURRENT_DIR/codeletset_load_request.json"
