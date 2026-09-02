## Gloox cpp library based headless XMPP Chatbot for Remote IT Administration and Automation

## Overview
This lightweight XMPP chatbot is built using the Gloox XMPP library and designed to facilitate remote IT administration and automation tasks. It provides a secure and efficient way to interact with your infrastructure using XMPP messages, enabling a wide range of capabilities such as reverse SSH tunneling, local AI model inference, and automated task execution.

## Features
- **BOSH Support (NEW)**: Bypass corporate firewalls by tunneling XMPP over HTTPS (port 443) instead of direct XMPP connection (port 5222). Perfect for restricted network environments.
- **Reverse SSH Tunneling**: Create secure tunnels to SSH servers, allowing remote access to the chatbot container and its host environment.
- **Local AI Model Inference**: Seamlessly integrate with locally hosted AI models (e.g., Ollama) to run inferences by sending chat messages to the XMPP chatbot.
- **Automation Capabilities**: Invoke predefined actions and automate tasks using simple XMPP messages.
- **Lightweight and Efficient**: Designed with minimal dependencies, ensuring a small container footprint (~21MB) and fast deployment.
- **Secure Communication**: Built on the Gloox XMPP library, supporting secure messaging protocols.

## Use Cases
- **Remote IT Administration**: Easily manage remote systems by invoking commands or automating actions via chat.
- **AI-Assisted Operations**: Interact with AI models locally hosted on the chatbot's system for insights or decision-making.
- **Infrastructure Management**: Execute commands or scripts on the chatbot’s host environment without direct shell access.
- **Automation**: Trigger workflows or tasks in your environment through simple XMPP messages.

## Getting Started

1. **Create a Directory for the Chatbot**:
   ```bash
   mkdir ~/xmpp-chatbot/ && cd ~/xmpp-chatbot
   ```

2. **Ensure You Have Two XMPP Accounts**:
   - One account for yourself (**admin buddy**).
   - A second account for the **chatbot**.

3. **Create `xmpp-login.txt` for the Chatbot**:
   ```bash
   echo "user: chatbot@myxmppserver" > xmpp-login.txt
   echo "pw: chatbotsecretpw" >> xmpp-login.txt
   echo "adminbuddy: me@myxmppserver" >> xmpp-login.txt
   ```

4. **Ensure the XMPP Server is Reachable**:
   ```bash
   ping myxmppserver
   ```

5. **Pull the Docker Image**:
   ```bash
   docker pull hackboxguy/xmpp-chatbot:latest
   ```

6. **Run the Container**:
   ```bash
   docker run -d \
       --name xmpp-chatbot \
       -v $(pwd)/xmpp-chatbot-data/ssh:/root/.ssh \
       -v $(pwd)/xmpp-login.txt:/run/secrets/xmpp-login \
       -e XMPP_LOGIN_FILE=/run/secrets/xmpp-login \
       --restart unless-stopped \
       --read-only \
       --tmpfs /tmp \
       --tmpfs /var/run \
       --tmpfs /etc/dropbear \
       --network host \
       hackboxguy/xmpp-chatbot:latest
   ```
   - Alternatively, use [this](https://github.com/hackboxguy/jsonrpc-tcp-srv/blob/main/docker-compose.yml) Docker Compose file:
     ```bash
     docker-compose up -d
     ```

7. **Log In to Your XMPP Server(from PC or from smartphone)**:
   - Use your favorite XMPP client app (e.g., Snikket, C0nnectPRO, Gajim, or Pidgin) to log in to your XMPP server.

8. **Connect with the Chatbot**:
   - If needed, send a friend request to `chatbot@myxmppserver`.
   - Start chatting by sending commands (e.g., send "**help**" to get the auto response of chatbot).

9. **Stop the Chatbot Container**:
   ```bash
   docker stop xmpp-chatbot
   ```

## Roles: who may run what

Every contact that the bot answers has a role:

- **admin**: the `adminbuddy` (and `bkupadminbuddy`) from `xmpp-login.txt`, plus anyone listed as admin in the ACL file. Can run everything, including `shellcmd`, `reboot`, buddy management and `acl`.
- **operator**: may run controls (gpio set, sonoff, display, sleep, identify) and read everything.
- **viewer**: the default for every other roster member. Read-only commands (status, lists, `help`).

Roles live in a file with one `jid role` per line, passed with `--aclfile` (the Docker image uses `/xmpp-data/xmpp-acl.txt`). Admins manage it over chat:

```
acl                                 list roles
acl family@myxmppserver operator    set a role (persisted)
acl family@myxmppserver remove      back to the default role
acl reload                          after editing the file by hand
```

A denied command answers `return=ActionBlocked : result=requires <role>`. Contacts that are not in the roster are ignored entirely.

## Machine-to-machine: JSON-RPC over XMPP

Apps talk to the bot with JSON-RPC 2.0 in ordinary XMPP message bodies. A body starting with `{` is a request, for example `{"jsonrpc":"2.0","id":1,"method":"exec","params":{"cmd":"uptime"}}`; `describe` and `list_commands` tell an app what the device offers, and asynchronous commands finish with a `task.done` notification. The full specification is in [docs/protocol.md](docs/protocol.md). Chat commands keep working unchanged.

## BOSH Connection (For Corporate Networks)

If your network blocks XMPP port 5222 but allows HTTPS (port 443), you can use BOSH (Bidirectional-streams Over Synchronous HTTP) to tunnel XMPP traffic over HTTPS.

### BOSH Configuration

**Standard Configuration (Direct XMPP):**
```
user: chatbot@myxmppserver.com
pw: secretpassword
adminbuddy: admin@myxmppserver.com
```

**BOSH Configuration (HTTPS Tunnel):**
```
user: chatbot@myxmppserver.com
pw: secretpassword
adminbuddy: admin@myxmppserver.com
bosh: true
boshurl: https://192.168.1.2:443/http-bind
boshhost: myxmppserver.com
tlsverify: false
```

**Configuration Parameters:**
- `bosh: true` - Enable BOSH mode
- `boshurl:` - BOSH endpoint URL (use IP address to bypass DNS blocking)
- `boshhost:` - Domain name for HTTP Host header and TLS SNI
- `tlsverify: false` - Disable certificate validation (useful for corporate networks with self-signed certs)

### Why Use IP Address in BOSH URL?

Corporate networks may block DNS resolution for XMPP domains. Using an IP address in `boshurl:` bypasses DNS blocking while still sending the correct domain name in the HTTP Host header via `boshhost:` parameter.

### Testing BOSH Endpoint

Before configuring the chatbot, verify the BOSH endpoint is accessible:

```bash
curl -v -k https://192.168.1.2:443/http-bind -H "Host: myxmppserver.com"
```

Expected response: `Prosody BOSH endpoint - It works!` or similar message from your XMPP server.

### Known Limitation

BOSH uses HTTP long-polling which introduces response delays (10-40 seconds) compared to direct TCP connections (~1 second). This is a fundamental limitation of the BOSH protocol and cannot be eliminated. However, the chatbot remains fully functional and can bypass restrictive firewalls.

**Comparison:**
- **Direct XMPP (port 5222)**: Fast (~1 second response)
- **BOSH (port 443)**: Delayed (~10-40 seconds response), but works through firewalls

### Docker Run with BOSH

When using BOSH, ensure your `xmpp-login.txt` includes the BOSH parameters, then run the container as usual:

```bash
docker run -d \
    --name xmpp-chatbot \
    -v $(pwd)/xmpp-login.txt:/run/secrets/xmpp-login \
    -e XMPP_LOGIN_FILE=/run/secrets/xmpp-login \
    --network host \
    hackboxguy/xmpp-chatbot:latest
```

The chatbot will automatically detect BOSH configuration and connect via HTTPS instead of direct XMPP.

## Additional Resources

10. **Top level sources for this container are available here: https://github.com/hackboxguy/jsonrpc-tcp-srv**
11. **Sources of xmpp-chatbot are at: https://github.com/hackboxguy/jsonrpc-tcp-srv/tree/main/services/xmproxy**
12. **Detailed BOSH implementation documentation: See [CLAUDE.md](../../CLAUDE.md#bosh-support-corporate-network-bypass)**

