CREATE TABLE IF NOT EXISTS assets (
    mac_address TEXT PRIMARY KEY,
    ip_addresses TEXT[] NOT NULL DEFAULT '{}',
    hostname TEXT,
    first_seen TEXT NOT NULL,
    last_seen TEXT NOT NULL,
    discovery_sources TEXT[] NOT NULL DEFAULT '{}',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
