#!/bin/bash

echo "Starting 5-minute task..."

# Duration: 5 minutes = 300 seconds
end=$((SECONDS+300))

while [ $SECONDS -lt $end ]; do
    echo "Still running at $(date)"
    sleep 30
done

echo "Task finished after 5 minutes."
