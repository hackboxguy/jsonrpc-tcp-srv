-- Prosody configuration for the xmproxy local test rig.
-- Throwaway server: plaintext c2s on 127.0.0.1:5222, no TLS, no registration.
-- Accounts are created by rig.sh with prosodyctl.
daemonize = false
pidfile = "/var/run/prosody/prosody.pid"
admins = { "admin@localhost" }

modules_enabled = {
    "roster"; "saslauth"; "disco"; "carbons"; "pep"; "private"; "blocklist";
    "version"; "uptime"; "time"; "ping"; "offline"; "admin_adhoc";
}
modules_disabled = { "tls"; "s2s"; "dialback"; "register"; }

allow_registration = false
c2s_require_encryption = false
allow_unencrypted_plain_auth = true
s2s_require_encryption = false
authentication = "internal_hashed"
c2s_ports = { 5222 }

-- generous limits: the stress test in bucket 1 sends bursts
limits = {}

log = {
    { levels = { min = "info" }, to = "console" };
}

VirtualHost "localhost"
