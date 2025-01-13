## Gloox cpp library based headless XMPP Chatbot for Remote IT Administration and Automation

## Overview
This lightweight XMPP chatbot is built using the Gloox XMPP library and designed to facilitate remote IT administration and automation tasks. It provides a secure and efficient way to interact with your infrastructure using XMPP messages, enabling a wide range of capabilities such as reverse SSH tunneling, local AI model inference, and automated task execution.

## Features
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
10. **Top level Sources for this container are available here: https://github.com/hackboxguy/jsonrpc-tcp-srv**:
11. **Sources of xmpp-chatbot are at: https://github.com/hackboxguy/jsonrpc-tcp-srv/tree/main/services/xmproxy**:
  
