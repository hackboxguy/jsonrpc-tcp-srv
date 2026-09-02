"""Minimal XMPP test client for the xmproxy rig (development only).

Wraps slixmpp so tests can do:

    async with BotChat("admin@localhost", "pw", "bot@localhost") as chat:
        replies = await chat.ask("account")

`ask` sends one chat message and collects every reply body until the bot has
been quiet for `quiet` seconds or `timeout` elapses. Asynchronous commands
produce more than one reply, so replies are returned as a list.
"""
import asyncio
import logging

from slixmpp import ClientXMPP

log = logging.getLogger("xmppclient")


class BotChat(ClientXMPP):
    def __init__(self, jid, password, bot_jid, host="127.0.0.1", port=5222,
                 plaintext=True):
        super().__init__(jid, password)
        self.bot_jid = bot_jid
        self._host = host
        self._port = port
        self._inbox = asyncio.Queue()
        self._ready = asyncio.Event()
        self.disconnects = 0
        self._closing = False
        if plaintext:
            # local rig: no TLS offered by the server
            self.enable_starttls = False
            self.enable_direct_tls = False
            self.enable_plaintext = True
            self.plugin["feature_mechanisms"].unencrypted_plain = True
        self.add_event_handler("session_start", self._on_start)
        self.add_event_handler("message", self._on_message)
        self.add_event_handler("failed_auth", self._on_failed_auth)
        self.add_event_handler("disconnected", self._on_disconnected)

    async def _on_start(self, _event):
        self.send_presence()
        await self.get_roster()
        self._ready.set()

    def _on_failed_auth(self, _event):
        log.error("authentication failed for %s", self.boundjid)
        self._ready.set()

    def _on_disconnected(self, _event):
        # unexpected drop (for example the server restarted): reconnect
        self.disconnects += 1
        if not self._closing:
            self._ready.clear()
            log.warning("%s disconnected, reconnecting", self.boundjid.bare)
            self.connect(host=self._host, port=self._port)

    async def wait_ready(self, timeout=30):
        await asyncio.wait_for(self._ready.wait(), timeout=timeout)

    def _on_message(self, msg):
        if msg["type"] in ("chat", "normal") and msg["body"]:
            self._inbox.put_nowait((msg["from"].bare, msg["body"]))

    async def __aenter__(self):
        self._closing = False
        self.connect(host=self._host, port=self._port)
        await asyncio.wait_for(self._ready.wait(), timeout=15)
        if not self.is_connected():
            raise RuntimeError("could not connect/authenticate %s" % self.boundjid)
        return self

    async def __aexit__(self, *exc):
        self._closing = True
        self.disconnect()
        await asyncio.sleep(0.2)

    def send_to_bot(self, body):
        self.send_message(mto=self.bot_jid, mbody=body, mtype="chat")

    async def collect(self, quiet=1.5, timeout=10.0, min_replies=1):
        """Gather reply bodies from the bot until quiet for `quiet` seconds."""
        replies = []
        loop = asyncio.get_event_loop()
        deadline = loop.time() + timeout
        while True:
            remaining = deadline - loop.time()
            if remaining <= 0:
                break
            wait = quiet if len(replies) >= min_replies else remaining
            try:
                sender, body = await asyncio.wait_for(self._inbox.get(), timeout=min(wait, remaining))
            except asyncio.TimeoutError:
                if len(replies) >= min_replies:
                    break
                continue
            if sender == self.bot_jid:
                replies.append(body)
        return replies

    async def ask(self, body, quiet=1.5, timeout=10.0, min_replies=1):
        # drain anything stale first
        while not self._inbox.empty():
            self._inbox.get_nowait()
        self.send_to_bot(body)
        return await self.collect(quiet=quiet, timeout=timeout, min_replies=min_replies)


async def _demo(argv):
    import sys
    jid, pw, bot, cmd = argv[1:5]
    async with BotChat(jid, pw, bot) as chat:
        for r in await chat.ask(cmd):
            print(r)


if __name__ == "__main__":
    import sys
    logging.basicConfig(level=logging.WARNING)
    asyncio.run(_demo(sys.argv))
