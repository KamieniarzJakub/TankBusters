#!/usr/bin/env bash
curl \
  -H "Email: {{ admin_email }}" \
  -H "Title: TankBusters server has failed" \
  -H "Tags: warning" \
  -d "$(systemctl status --full '{{ service_name }}.service')" \
  "https://ntfy.sh/{{ ntfy_topic }}"
