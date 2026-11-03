# Running a server with HTTPS

Every dedicated server runs the **Web API**: a small web server alongside the game, serving the server's status, players, moderation routes and a live Event feed. It speaks plain HTTP out of the box. Give it a certificate and the same port also speaks HTTPS and `wss://`. This guide covers turning that on, getting a free certificate from Let's Encrypt with `acme.sh`, and keeping it renewed.

For what the Web API offers, see `doc/web-api-guide.md`. For how it works inside, see `doc/web-api.md`.

## Do you need it?

Your server works without a certificate:

- **Players** see your server, its status and its players either way.
- **Websites**, including redeclipse.net, reach your server through the master's **relay**, which needs no certificate.
- **The game client** checks a server's certificate by the fingerprint the master lists, so a self-signed certificate is enough for it.

A certificate is worth having when:

- You want to use **API tokens** or **sign in** from anywhere other than the server itself. Routes above Player, `POST /auth` and signing in on the Event feed are refused over plain HTTP, except from localhost and trusted proxies, because a token sent in the clear works for whoever sees it.
- You want **browsers** or **tools** to reach your server directly, without the relay. Browsers only accept a certificate signed by a known authority (such as Let's Encrypt) for a domain name that points at your server.

| You want | Certificate |
|---|---|
| Moderation tools and bots of your own | Self-signed is fine, as long as the tools trust its fingerprint |
| Browsers and anyone's tools reaching you directly | Let's Encrypt, for a domain name |
| Only the public routes, through redeclipse.net | None |

## Before you start

- **The port.** The Web API listens on TCP port **28888** by default. If that's taken, as with a second server on the same machine, it tries the next 16 ports in turn and tells the master which one it got. Open the port in your firewall for **TCP** (the game itself uses UDP 28801 and 28802). To make the port predictable for a firewall rule, fix it with `httpserverport` and `httpserverportrange 0` (see below).
- **TLS support.** Builds include TLS unless they were made with `WANT_TLS=0`. The official builds include it.
- **For Let's Encrypt, a domain name.** You need a name such as `re.example.org` whose DNS `A` (and `AAAA`, for IPv6) record points at your server. Let's Encrypt doesn't issue certificates for bare IP addresses in the way this guide uses.

## The settings

These go in your `servinit.cfg` (see `doc/examples/servinit.cfg`):

| Setting | Default | Meaning |
|---|---|---|
| `httpserver` | 1 on dedicated servers | Turns the Web API on (1) or off (0) |
| `httpserverport` | 28888 | The first port to try |
| `httpserverportrange` | 16 | How many ports after it to try; 0 fixes the port |
| `httpserverip` | (all) | The address to listen on |
| `httpcertfile` | (none) | The certificate, in PEM. Setting it turns HTTPS on |
| `httpkeyfile` | (none) | The private key, in PEM. Leave it empty if the key is in the certificate file |
| `httprelay` | 1 | What the master may relay: 0 nothing, 1 public routes and the Event feed, 2 everything |
| `httptrustproxy` | (none) | Addresses of reverse proxies whose `X-Forwarded-For` is believed |
| `httpverbose` | 0 | 1 logs requests, 2 also logs refused connections and failed TLS handshakes |

`httpcertfile` should hold your certificate followed by any intermediate certificates (the "full chain"). Use **absolute paths**: they are read relative to the directory the server was started in, not its home directory.

You don't need to restart the server when the certificate changes. About once a minute the server checks both files and loads them again when either has changed, and it tells the master the new fingerprint the next time it registers, within five minutes. If the new files can't be loaded, the server logs why and keeps serving the old certificate.

## Option 1: a self-signed certificate

This takes one command and doesn't need a domain name. Browsers will refuse it, but the game client and your own tools can trust it by its fingerprint.

```sh
mkdir -p ~/re-certs
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -days 3650 \
    -keyout ~/re-certs/key.pem -out ~/re-certs/cert.pem -subj "/CN=re.example.org"
chmod 600 ~/re-certs/key.pem
```

Then, in `servinit.cfg` (with your own home directory in the paths):

```
httpcertfile "/home/re/re-certs/cert.pem"
httpkeyfile "/home/re/re-certs/key.pem"
```

To print the fingerprint for your tools (the server also logs it at startup):

```sh
openssl x509 -in ~/re-certs/cert.pem -noout -fingerprint -sha256 | cut -d= -f2 | tr -d : | tr A-F a-f
```

## Option 2: Let's Encrypt with acme.sh

[acme.sh](https://github.com/acmesh-official/acme.sh) is a shell script that asks Let's Encrypt for certificates and renews them from cron. It runs as an ordinary user, so you can run it as the same user as the game server, and the server can then read the files it writes.

### 1. Install acme.sh

As the user that runs your server:

```sh
curl https://get.acme.sh | sh -s email=you@example.org
```

Log out and back in (or `source ~/.bashrc`) so the `acme.sh` command is on your path. The installer also adds a daily cron job that renews certificates when they are due.

acme.sh uses ZeroSSL by default. To use Let's Encrypt, as this guide does:

```sh
acme.sh --set-default-ca --server letsencrypt
```

### 2. Prove you own the domain

Let's Encrypt has to check that the name is yours before it signs a certificate. Pick one way:

**HTTP on port 80 (standalone).** acme.sh briefly runs its own web server on port 80. Port 80 must be open and nothing else may be using it, and binding to it usually needs root, so run this step with `sudo` or give the user permission to bind low ports.

```sh
acme.sh --issue --standalone -d re.example.org --keylength ec-256
```

**An existing web server (webroot).** If the machine already serves a website for that name on port 80, acme.sh can drop its proof in the site's directory:

```sh
acme.sh --issue -w /var/www/html -d re.example.org --keylength ec-256
```

**DNS.** acme.sh adds a TXT record through your DNS provider's API. This needs no open ports and no root, so it suits shared hosts and servers behind other firewalls. Each provider has its own plugin and credentials; see the [list of DNS APIs](https://github.com/acmesh-official/acme.sh/wiki/dnsapi). For example, with Cloudflare:

```sh
export CF_Token="<Cloudflare API token with DNS edit access>"
acme.sh --issue --dns dns_cf -d re.example.org --keylength ec-256
```

acme.sh saves the credentials in `~/.acme.sh/account.conf` and reuses them when it renews.

`--keylength ec-256` asks for an ECDSA certificate. Use it: TLS handshakes with ECDSA cost the server far less than with RSA.

While you're testing, add `--staging` to any of these. Let's Encrypt limits how many real certificates you can ask for in a week, and staging doesn't count against that (browsers won't trust a staging certificate). Drop `--staging` and add `--force` once it works.

### 3. Install the certificate where the server reads it

Don't point the server at the files in `~/.acme.sh/`, which may change layout. Have acme.sh copy them to a directory of your own, now and after every renewal:

```sh
mkdir -p ~/re-certs
acme.sh --install-cert -d re.example.org --ecc \
    --fullchain-file ~/re-certs/fullchain.pem \
    --key-file ~/re-certs/key.pem
```

Use `--fullchain-file`, not `--cert-file`: without the intermediate certificate, browsers and tools can't check yours.

No `--reloadcmd` is needed, as the server picks up the new files by itself.

If acme.sh runs as a different user from the server (for example as root), make sure the server's user can read both files, and that no one else can read the key.

### 4. Point the server at it

In `servinit.cfg`:

```
httpcertfile "/home/re/re-certs/fullchain.pem"
httpkeyfile "/home/re/re-certs/key.pem"
```

Start the server, or if it's already running, `rehash` loads the new settings.

### Renewal

Let's Encrypt certificates last 90 days. acme.sh's cron job renews them about 30 days before they expire and copies the new files into place, and the server loads them within a minute. To check what acme.sh knows and when it will renew:

```sh
acme.sh --list
```

To try a renewal now, without waiting:

```sh
acme.sh --renew -d re.example.org --ecc --force
```

## Checking it works

When the server loads the certificate it logs:

```
TLS: serving HTTPS with /home/re/re-certs/fullchain.pem (sha256 9f86d0...)
HTTP server started on *:[28888]
```

The port in brackets is the one it got. From another machine:

```sh
curl https://re.example.org:28888/api/v1
```

should print the server's name, version and routes. For a self-signed certificate, add `--cacert ~/re-certs/cert.pem` and use the name you gave in `-subj`, or `-k` to skip the check.

To see what certificate the server presents:

```sh
openssl s_client -connect re.example.org:28888 -servername re.example.org </dev/null 2>/dev/null | openssl x509 -noout -subject -enddate
```

## Troubleshooting

| Log line | What to do |
|---|---|
| `TLS: unable to read <cert> or <key>` | A file is missing, or the server can't read it. Check the paths are absolute and the server's user can read both |
| `TLS: unable to load certificate <file>: ...` | The file isn't a PEM certificate. Use the full chain acme.sh installed, not the CSR or the key |
| `TLS: unable to load key <file>: ...` | The key file isn't a PEM private key, or it's encrypted. The server can't use a key with a passphrase |
| `TLS: certificate <cert> does not match key <key>` | The two files come from different certificates, often an old key left beside a new certificate. Run `acme.sh --install-cert` again |
| `Failed to start the HTTP server on *:28888 or the ports after it` | Every port in the range is taken. Pick another `httpserverport` |

If nothing connects at all, check the firewall allows **TCP** on the port the log reports, and that your DNS name points at this machine. `httpverbose 2` logs refused connections and failed handshakes, which shows whether connections arrive at all.

Browsers saying the certificate is for the wrong name means the name in the address bar isn't on the certificate; use the name you issued it for. Browsers saying it isn't trusted usually means it's self-signed, from staging, or installed without its intermediate certificate.

## Using a reverse proxy instead

If you already run nginx, Caddy or similar with certificates of their own, they can carry the TLS instead. Leave `httpcertfile` empty, have the proxy forward to the Web API's port over plain HTTP, and list the proxy's address in `httptrustproxy` so the server believes the `X-Forwarded-For` and `X-Forwarded-Proto` headers it sends:

```
httptrustproxy "127.0.0.1"
```

Requests through a trusted proxy count as secure when the proxy sends `X-Forwarded-Proto: https`, so tokens and signing in work through it. In nginx that's `proxy_set_header X-Forwarded-Proto $scheme;` and `proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;`. Proxy the Event feed's WebSocket upgrade too (in nginx, `proxy_http_version 1.1` with the `Upgrade` and `Connection` headers passed on). The master's list will show no fingerprint for your server, so the game client talks to it over plain HTTP, which is fine for the public routes it uses.

## Keep the key private

Anyone with your private key can pretend to be your server. Keep it readable only by the server's user (`chmod 600`), keep it out of public config repositories, and if it leaks, issue a new certificate with a new key (`acme.sh --issue ... --force` makes a new key by default) and install it.
