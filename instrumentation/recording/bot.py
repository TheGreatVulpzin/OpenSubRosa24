"""A bot player for the original server. It joins like a client and can move its body into a base.

The bot sends the join and input packets of tools/net/srpacket.py. It reads and writes the game
memory of the server process, so it can put its human inside the box of a base. A player without a
team (team 7) whose human is inside a base joins that team, and becomes its manager when the team
has none. This is the way to reach company code (missions, guards, stocks) without a person.

  python -I instrumentation/recording/bot.py --pid 1234 --company 0 --stay 60
  python -I instrumentation/recording/bot.py --pid 1234 --account 7 8 --name Bot1 --stay 20 --leave

Arguments:
  --pid N            process of the original server (needed for --company and --snapshot)
  --name, --account  player name, and the two numbers of the account id (a new pair makes a new account)
  --company T        World mode: put the human inside the base of team T (0 to 4)
  --shop             with --company: use the menus of the base (items, weapons, stocks, bank)
  --exit-company     after the stay, move out of the base with the leave button down (team 7 again)
  --team N           Round mode: the team choice of menu 2 (1 to 3). The bot then marks itself ready
  --stay S           seconds to play
  --snapshot S       S seconds after the join, ask the recorder for the snapshot (snapshot_now.txt)
  --leave            send the disconnect at the end. Without it the server drops the bot after 20 s
  --rejoin S         join again with the same account after S seconds (inventory load and save)
  --viz              draw the world in the terminal (tools/net/viz.py)
  --console          mainframe style console with panels (tools/net/mainframe.py), --no-boot skips the boot text

Only the original server (fixed image base 0x20000000) is supported. Run it on the test server of
the recorder, never on a server with real players.
"""
import argparse
import ctypes
import os
import socket
import struct
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'net'))
import srpacket  # noqa: E402

BASE = 0x20000000
GAME = 0x90DC540            # game record
GAME_TEAM_STRIDE = 0x117C
GAME_SAFE_MIN = 0x70        # team 0 safe box, vec3 min then vec3 max
GAME_SAFE_MAX = 0x7C
GAME_MANAGER = 0x9C
PLAYER = 0xEDEDD00
PLAYER_STRIDE = 0x2408
PLAYER_TEAM = 0x64
PLAYER_MENU = 0xD4
HUMAN = 0x15CBF80
HUMAN_STRIDE = 0x2B64
HUMAN_POS = 0x48
HUMAN_BONES = 0xC8
BONE_STRIDE = 0xE0
CITY = 0xE299220
CITY_NUM_BUILDINGS = 0x2F998
CITY_BUILDING_TYPE = 0x2F9DC     # building records are 0x9dc bytes
CITY_BUILDING_MIN = 0x2F9F8
CITY_BUILDING_MAX = 0x2FA04
CITY_SHOP_COUNT = 0x302A4
BUILDING_STRIDE = 0x9DC
BUTTON_LEAVE_BASE = 0x40000
# (menu, value): the menu of the player must be the first number when the value goes out. 16 goes back to menu 3.
SHOP = ([(3, 1)] + [(4, v) for v in (1, 2, 3, 4, 16)] +
        [(3, 2)] + [(5, v) for v in (1, 2, 3, 4, 16)] +
        [(3, 3), (7, 1), (7, 2), (7, 16)] +
        [(3, 5)] + [(8, v) for v in (1, 3, 5, 2, 4, 16)] +
        [(3, 4)] + [(11, v) for v in (1, 2, 7, 16)])

LOG = None          # list of lines while the console screen is on, else messages go to stdout


def say(text):
    if LOG is None:
        print(text)
    else:
        LOG.append(str(text))
        del LOG[:-40]


k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenProcess.restype = wintypes.HANDLE
k32.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.POINTER(ctypes.c_size_t)]
k32.WriteProcessMemory.argtypes = k32.ReadProcessMemory.argtypes


class Memory:
    def __init__(self, pid):
        self.handle = k32.OpenProcess(0x0010 | 0x0020 | 0x0008 | 0x0400, False, pid)
        if not self.handle:
            raise SystemExit('cannot open process %d (error %d)' % (pid, ctypes.get_last_error()))

    def read(self, rva, size):
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t(0)
        if not k32.ReadProcessMemory(self.handle, ctypes.c_void_p(BASE + rva), buf, size, ctypes.byref(got)):
            raise OSError('read of RVA 0x%x failed' % rva)
        return buf.raw

    def write(self, rva, data):
        got = ctypes.c_size_t(0)
        if not k32.WriteProcessMemory(self.handle, ctypes.c_void_p(BASE + rva), data, len(data), ctypes.byref(got)):
            raise OSError('write of RVA 0x%x failed' % rva)

    def int(self, rva):
        return struct.unpack('<i', self.read(rva, 4))[0]

    def vec3(self, rva):
        return struct.unpack('<3f', self.read(rva, 12))


def process_folder(pid):
    """The folder of the program of a process."""
    handle = k32.OpenProcess(0x1000, False, pid)        # PROCESS_QUERY_LIMITED_INFORMATION
    buf = ctypes.create_unicode_buffer(1024)
    size = wintypes.DWORD(1024)
    ok = k32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size))
    k32.CloseHandle(handle)
    if not ok:
        raise OSError('cannot read the path of process %d' % pid)
    return str(Path(buf.value).parent)


def find_pid():
    out = subprocess.run(['tasklist', '/fo', 'csv', '/nh', '/fi', 'imagename eq subrosadedicated024d.exe'],
                         capture_output=True, text=True).stdout
    pids = [int(line.split('","')[1]) for line in out.splitlines() if line.startswith('"')]
    if len(pids) != 1:
        raise SystemExit('found %d server processes: give --pid' % len(pids))
    return pids[0]


def team_box(mem, team):
    base = GAME + team * GAME_TEAM_STRIDE
    return mem.vec3(base + GAME_SAFE_MIN), mem.vec3(base + GAME_SAFE_MAX)


def move_human(mem, human, target):
    """Shift the position, the old position and every bone by the same distance.
    The bones move by Verlet steps (position minus old position), so both must shift."""
    base = HUMAN + human * HUMAN_STRIDE
    pos = mem.vec3(base + HUMAN_POS)
    delta = tuple(t - p for t, p in zip(target, pos))
    offsets = [HUMAN_POS, HUMAN_POS + 12]
    for i in range(15):
        bone = HUMAN_BONES + i * BONE_STRIDE
        offsets += [bone, bone + 12]
    for offset in offsets:
        cur = mem.vec3(base + offset)
        mem.write(base + offset, struct.pack('<3f', *[c + d for c, d in zip(cur, delta)]))
    zero = struct.pack('<3f', 0, 0, 0)
    for i in range(15):
        mem.write(base + HUMAN_BONES + i * BONE_STRIDE + 0x18, zero)


class Bot:
    def __init__(self, args):
        self.args = args
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setblocking(False)
        self.addr = (args.host, args.port)
        self.game_id = None
        self.state = None
        self.actions = []           # every action made so far, in order
        self.acked = 0
        self.last_menu_action = 0.0
        self.event_ack = 0
        self.gametype = None
        self.viewer = None          # terminal view of the world (tools/net/viz.py), set by --viz
        self.boxes = []
        self.last_draw = 0.0
        self.team_picked = False
        self.console = None         # tools/net/mainframe.py, set by --console
        self.mem = None
        self.calls = {}
        self.calls_pos = 0
        self.calls_path = None
        self.names = None
        self.tick_mark = (time.time(), 0)
        self.tps = 0.0
        self.snapshot_at = None

    def send(self, packet):
        self.sock.sendto(srpacket.build_packet(packet), self.addr)

    def receive(self):
        while True:
            try:
                data, _ = self.sock.recvfrom(65536)
            except (BlockingIOError, ConnectionResetError):
                return
            try:
                pkt = srpacket.parse_packet(data)
            except srpacket.PacketError:
                continue
            if isinstance(pkt, srpacket.NewGame):
                self.game_id, self.gametype = pkt.game_id, pkt.gametype
            elif isinstance(pkt, srpacket.State) and pkt.game_id == self.game_id:
                self.state = pkt
                self.acked = pkt.action_ack
                self.event_ack = (pkt.first_event + len(pkt.events)) & 0xFFFF

    def join(self):
        pkt = srpacket.Join(srpacket.PROTOCOL_VERSION, self.args.name.encode(), tuple(self.args.account))
        deadline = time.time() + 10
        while self.game_id is None and time.time() < deadline:
            self.send(pkt)
            time.sleep(0.1)
            self.receive()
        if self.game_id is None:
            raise SystemExit('the server did not answer the join')

    def tick(self, buttons=0):
        pending = self.actions[self.acked:self.acked + 15]
        voice = [srpacket.VoiceFrame(i, 0, b'') for i in range(6)]
        pkt = srpacket.Input(game_id=self.game_id, buttons=buttons, event_ack=self.event_ack,
                             action_start=self.acked, actions=pending, voice=voice)
        self.send(pkt)
        self.receive()
        self.draw()
        if self.snapshot_at is not None and time.time() >= self.snapshot_at:
            # The recorder of the server takes the snapshot when this file appears.
            open(os.path.join(self.args.folder, 'snapshot_now.txt'), 'w').close()
            self.snapshot_at = None
            say('snapshot requested')

    def draw(self):
        now = time.time()
        if self.viewer is None or self.state is None or now - self.last_draw < 0.05:
            return
        self.last_draw = now
        me = self.state.human if self.state.human != -1 else None
        if self.console is not None:
            frame = self.console.render(self.state, self.boxes, me=me, extra=self.extra())
        else:
            frame = self.viewer.render(self.state, self.boxes, me=me)
        sys.stdout.write('\x1b[H' + frame)
        sys.stdout.flush()

    def count_calls(self):
        """Count the calls that the recorder of the server has written since the last look."""
        if self.calls_path is None or not os.path.exists(self.calls_path):
            return
        if self.names is None:
            import csv
            table = Path(__file__).resolve().parents[2] / 'analysis' / '024d' / 'functions.csv'
            with open(table, encoding='utf-8') as handle:
                self.names = {int(r['rva'], 16): r['name'] for r in csv.DictReader(handle)}
        with open(self.calls_path, 'rb') as handle:
            handle.seek(self.calls_pos)
            data = handle.read(2 << 20)
        end = data.rfind(b'\n') + 1
        self.calls_pos += end
        for line in data[:end].decode('ascii', 'replace').splitlines():
            if line.startswith('R '):
                name = self.names.get(int(line.split()[1], 16))
                if name:
                    self.calls[name] = self.calls.get(name, 0) + 1

    def extra(self):
        st = self.state
        now = time.time()
        if now - self.tick_mark[0] >= 1.0:
            self.tps = (st.tick - self.tick_mark[1]) / (now - self.tick_mark[0])
            self.tick_mark = (now, st.tick)
            self.count_calls()
        data = {'tps': self.tps, 'clock': st.world, 'log': list(LOG or ())[-8:], 'calls': self.calls,
                'recorder': self.calls_path is not None, 'bot': self.args.name}
        if self.mem is not None:
            data['teams'] = [{'manager': self.mem.int(GAME + t * GAME_TEAM_STRIDE + GAME_MANAGER),
                              'members': 0, 'missions': 0} for t in range(5)]
        return data

    def menu(self, value):
        self.actions.append(srpacket.Action(srpacket.ACTION_MENU, self.state.menu, value))

    def spawn_if_needed(self):
        """Answer the menus that start play. World: menu 1 spawns. Round: menu 2 picks a team, then ready."""
        st = self.state
        if st is None or time.time() - self.last_menu_action < 1.0 or self.acked < len(self.actions):
            return
        if st.menu == 1 and st.human == -1:
            self.menu(1)
        elif st.menu == 2 and self.gametype != 4:
            self.menu(5 if self.team_picked else self.args.team)
            self.team_picked = not self.team_picked
        else:
            return
        self.last_menu_action = time.time()

    def script(self, steps):
        """Send menu choices: each (menu, value) waits for that menu of the player first."""
        for menu, value in steps:
            deadline = time.time() + 3
            while time.time() < deadline and not (self.state and self.state.menu == menu):
                self.run_for(0.05)
            if self.state.menu != menu:
                continue
            self.menu(value)
            self.run_for(0.4)

    def run_for(self, seconds, buttons=0):
        end = time.time() + seconds
        while time.time() < end:
            self.tick(buttons)
            self.spawn_if_needed()
            time.sleep(1 / 60)


def tour(bot, mem, human, home, per_type=3):
    """Visit shops (2 car dealer, 4 suits, 7 gun store) and banks (5): the human stands inside the building."""
    seen = {}
    for b in range(mem.int(CITY + CITY_NUM_BUILDINGS)):
        kind = mem.int(CITY + CITY_BUILDING_TYPE + b * BUILDING_STRIDE)
        if kind not in (2, 4, 5, 7) or seen.get(kind, 0) >= per_type:
            continue
        seen[kind] = seen.get(kind, 0) + 1
        lo = mem.vec3(CITY + CITY_BUILDING_MIN + b * BUILDING_STRIDE)
        hi = mem.vec3(CITY + CITY_BUILDING_MAX + b * BUILDING_STRIDE)
        # A menu opens only when the menu of the player is 0, so the human leaves the base first.
        move_human(mem, human, home)
        bot.run_for(0.6)
        move_human(mem, human, tuple((a + c) / 2 for a, c in zip(lo, hi)))
        bot.run_for(1.0)
        if kind == 5:
            bot.script([(12, v) for v in (1, 2, 7, 16)])
        else:
            rows = min(mem.int(CITY + CITY_SHOP_COUNT + b * BUILDING_STRIDE), 4)
            bot.script([(9, 1)] + [(10, v) for v in range(1, rows + 1)])
        say('building %d, type %d' % (b, kind))


def player_team(mem, slot):
    return mem.int(PLAYER + slot * PLAYER_STRIDE + PLAYER_TEAM)


def wait_for_human(bot):
    deadline = time.time() + 90
    while time.time() < deadline and (bot.state is None or bot.state.human == -1):
        bot.tick()
        bot.spawn_if_needed()
        time.sleep(1 / 60)
    if bot.state is None or bot.state.human == -1:
        raise SystemExit('the bot has no human (menu %s)' % (bot.state.menu if bot.state else None))
    return bot.state.player, bot.state.human


def wait_for_body(bot, mem, human):
    """The spawn writes the position a few ticks after the human appears."""
    hpos = HUMAN + human * HUMAN_STRIDE + HUMAN_POS
    while mem.vec3(hpos) == (0.0, 0.0, 0.0):
        bot.run_for(0.1)
    bot.run_for(1.5)
    return hpos


def session(args, mem):
    bot = Bot(args)
    bot.mem = mem
    if args.console:
        import mainframe
        global LOG
        LOG = []
        sys.stdout.reconfigure(encoding='utf-8')
        bot.console = mainframe.Mainframe()
        bot.viewer = bot.console.viewer
        bot.calls_path = os.path.join(args.folder, 'sv_args.txt') if args.folder else None
        if not args.no_boot:
            bot.console.boot(sys.stdout)
    if args.viz or args.console:
        if bot.viewer is None:
            import viz
            sys.stdout.reconfigure(encoding='utf-8')
            bot.viewer = viz.Viewer()
        if mem is not None:
            for team in range(5):
                lo, hi = team_box(mem, team)
                bot.boxes.append((lo, hi, 'base %d' % team, 208 if team == args.company else 244))
        sys.stdout.write('\x1b[2J\x1b[?25l')
    bot.join()
    if args.snapshot is not None:
        bot.snapshot_at = time.time() + args.snapshot
    say('joined: game id %d, game type %d' % (bot.game_id, bot.gametype))
    slot, human = wait_for_human(bot)
    say('player %d, human %d' % (slot, human))

    if args.company is not None:
        team = args.company
        lo, hi = team_box(mem, team)
        hpos = wait_for_body(bot, mem, human)
        home = mem.vec3(hpos)
        move_human(mem, human, tuple((a + b) / 2 for a, b in zip(lo, hi)))
        for _ in range(20):
            bot.run_for(0.1)
            if player_team(mem, slot) == team:
                break
        say('team of the bot: %d, manager of that team: %d' % (
            player_team(mem, slot), mem.int(GAME + team * GAME_TEAM_STRIDE + GAME_MANAGER)))
        if args.shop:
            bot.script(SHOP)
            tour(bot, mem, human, home)
            move_human(mem, human, tuple((a + c) / 2 for a, c in zip(lo, hi)))
        bot.run_for(args.stay)
        if args.exit_company:
            # The button makes the server count the ticks outside the base (180 ticks, 3 s).
            move_human(mem, human, home)
            bot.run_for(5, BUTTON_LEAVE_BASE)
            say('team after leaving the base: %d' % player_team(mem, slot))
    else:
        bot.run_for(args.stay)

    if args.leave:
        bot.send(srpacket.Disconnect())
        if args.viz or args.console:
            sys.stdout.write('\x1b[?25h\n')
        say('disconnect sent')
        time.sleep(0.5)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='127.0.0.1')
    ap.add_argument('--port', type=int, default=27584)
    ap.add_argument('--pid', type=int)
    ap.add_argument('--name', default='Bot')
    ap.add_argument('--account', type=int, nargs=2, default=[int(time.time()) & 0x7FFFFFFF, 12345])
    ap.add_argument('--company', type=int, help='move the human into the base of this team (0 to 4)')
    ap.add_argument('--stay', type=float, default=10, help='seconds to stay after the move')
    ap.add_argument('--shop', action='store_true', help='with --company: buy in the menus of the base')
    ap.add_argument('--exit-company', action='store_true', help='after the stay, leave the base and the team')
    ap.add_argument('--leave', action='store_true', help='send a disconnect at the end (else only stop)')
    ap.add_argument('--console', action='store_true', help='the world and the server in a mainframe style console')
    ap.add_argument('--no-boot', action='store_true', help='with --console: skip the boot text')
    ap.add_argument('--viz', action='store_true', help='draw the world in the terminal (ANSI, 3D wireframe)')
    ap.add_argument('--snapshot', type=float, help='seconds after the join: ask the recorder for the snapshot')
    ap.add_argument('--team', type=int, default=1, help='round mode: menu choice of the team (1 to 3)')
    ap.add_argument('--rejoin', type=float, help='join again with the same account after this many seconds')
    args = ap.parse_args()

    pid = args.pid or (find_pid() if args.company is not None or args.snapshot is not None or args.console else None)
    mem = Memory(pid) if args.company is not None or args.console else None
    args.folder = process_folder(pid) if args.snapshot is not None or args.console else None
    session(args, mem)
    if args.rejoin is not None:
        time.sleep(args.rejoin)
        args.leave, args.rejoin, args.stay, args.exit_company = True, None, 10, False
        session(args, mem)


if __name__ == '__main__':
    main()
