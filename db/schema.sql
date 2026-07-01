CREATE TABLE IF NOT EXISTS assets (
    mac_address TEXT PRIMARY KEY,
    ip_addresses TEXT[] NOT NULL DEFAULT '{}',
    hostname TEXT,
    first_seen TEXT NOT NULL,
    last_seen TEXT NOT NULL,
    discovery_sources TEXT[] NOT NULL DEFAULT '{}',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS asset_events (
    id BIGSERIAL PRIMARY KEY,
    event_time TEXT NOT NULL,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    ip_address TEXT,
    mac_address TEXT,
    old_ip TEXT,
    new_ip TEXT,
    old_mac TEXT,
    new_mac TEXT,
    hostname TEXT,
    protocol TEXT,
    interface TEXT,
    message TEXT,
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
