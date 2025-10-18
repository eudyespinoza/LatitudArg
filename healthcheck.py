import os
import socket
import sys
import urllib.request
import urllib.error


def main() -> int:
    # Short timeout to fail fast during container healthcheck
    socket.setdefaulttimeout(2)
    url = os.getenv("HEALTHCHECK_URL", "http://127.0.0.1:8001/login")
    try:
        with urllib.request.urlopen(url) as resp:
            # Consider any 5xx as unhealthy
            if 500 <= getattr(resp, "status", 200) < 600:
                return 1
            return 0
    except Exception:
        return 1


if __name__ == "__main__":
    sys.exit(main())
