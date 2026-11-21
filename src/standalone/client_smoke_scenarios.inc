/* Scenario controls use only SDL events and the polling wrappers. */
static int leave_game(ScreenCheck destination);

static int selected_shot(const char *name)
{
    char normalized[96];
    const char *list = getenv("SR_SMOKE_SHOTS"), *p;
    size_t length;
    slug(normalized, sizeof normalized, name); name = normalized; length = strlen(name);
    if (!list || !strcmp(list, "*")) return 1;
    for (p = list; *p;) {
        const char *end = strchr(p, ',');
        size_t size = end ? (size_t)(end - p) : strlen(p);
        if (size == length && !memcmp(name, p, length)) return 1;
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

static int shot(const char *name)
{
    DWORD start = GetTickCount();
    unsigned request;
    if (!selected_shot(name)) return 1;
    EnterCriticalSection(&lock);
    slug(shot_name, sizeof shot_name, name);
    request = ++shot_request;
    LeaveCriticalSection(&lock);
    while (GetTickCount() - start < FRAME_MS) {
        int done, ok;
        EnterCriticalSection(&lock);
        done = shot_done == request; ok = shot_ok;
        LeaveCriticalSection(&lock);
        if (done) return ok;
        if (WaitForSingleObject(stop, 0) == WAIT_OBJECT_0) return 0;
        if (!strcmp(scenario, "explore") && GetTickCount() - scenario_start >= EXPLORE_MS)
            return fail("explore capture exceeds 90 seconds");
        WaitForSingleObject(changed, 20);
    }
    return fail("capture timeout");
}

static int record_screen(const char *name, unsigned depth)
{
    SmokeFrame view = snapshot();
    int i, count = 0;
    if (screen_count++) fputs(",\n", screens);
    fputs("{\"name\":", screens); json_string(screens, name);
    fprintf(screens, ",\"depth\":%u,\"buttons\":[", depth);
    for (i = 0; i < view.count; i++) {
        const SmokeItem *item = &view.items[i];
        if (item->hidden || item->width <= 0 || item->height <= 0) continue;
        if (count++) fputc(',', screens);
        fputs("{\"text\":", screens); json_string(screens, item->text);
        fprintf(screens, ",\"kind\":%d,\"disabled\":%s,\"value\":", item->kind, item->disabled ? "true" : "false");
        json_string(screens, item->value_text); fputc('}', screens);
    }
    fputs("]}\n]", screens); fflush(screens); fseek(screens, -1, SEEK_CUR);
    return !ferror(screens) || fail("screen manifest write");
}

static unsigned screen_key(const SmokeFrame *view)
{
    unsigned hash = 2166136261u;
    int i;
    if (main_screen(view)) return 1;
    if (options_screen(view)) return 2;
    if (host_screen(view)) return 3;
    if (join_screen(view)) return 4;
    if (list_screen(view)) return 5;
    if (game_screen(view)) return 6;
    if (item_index(view, "Voice Chat") >= 0 && item_index(view, "Back") >= 0) return 7;
    for (i = 0; i < view->count; i++) {
        const unsigned char *p = (const unsigned char *)view->items[i].text;
        if (view->items[i].hidden) continue;
        while (*p) hash = (hash ^ *p++) * 16777619u;
        hash = (hash ^ (unsigned)view->items[i].kind) * 16777619u;
    }
    return hash;
}

static int text_test(const SmokeItem *item, const char *name)
{
    SmokeFrame view;
    int index;
    if (!click(item->text)) return 0;
    if (!key_press(KEY_HOME)) return 0;
    InterlockedExchange(&extra_key, KEY_SHIFT);
    index = key_press(KEY_END);
    InterlockedExchange(&extra_key, 0);
    if (!index) return 0;
    if (item->field_type) { if (!key_press(KEY_7)) return 0; }
    else if (!key_press(KEY_S) || !key_press(KEY_M) || !key_press(KEY_K)) return 0;
    if (!key_press(KEY_RETURN)) return 0;
    view = snapshot();
    index = item_index(&view, item->text);
    if (!shot(name) || !record_screen(name, 0)) return 0;
    if (index < 0 || (item->field_type ? strtod(view.items[index].value_text, NULL) != 7.0 : strcmp(view.items[index].value_text, "smk"))) {
        fprintf(stderr, "SMOKE TEXT field=%s expected=%s actual=%s\n", item->text,
                item->field_type ? "7" : "smk", index < 0 ? "missing" : view.items[index].value_text);
        return fail("typed text absent from menu field");
    }
    fprintf(stderr, "SMOKE ASSERT text PASS field=%s value=%s\n", item->text, view.items[index].value_text);
    return 1;
}

static int forbidden_button(const char *text)
{
    char lower[64];
    unsigned i;
    for (i = 0; text[i] && i + 1 < sizeof lower; i++)
        lower[i] = text[i] >= 'A' && text[i] <= 'Z' ? text[i] + 'a' - 'A' : text[i];
    lower[i] = 0;
    return !strcmp(lower, "exit") || !strcmp(lower, "quit") || !strcmp(lower, "back") ||
           strstr(lower, "delete") || strstr(lower, "remove") || strstr(lower, "save") ||
           strstr(lower, "reset") || strstr(lower, "new ") || strstr(lower, "apply") ||
           strstr(lower, "cpu affinity") || strstr(lower, "twitter") || strstr(lower, "website") || strstr(lower, "crypticsea");
}

/* Each child returns to its parent before the next visible button is tested. */
static int explore(unsigned depth, const char *name)
{
    static unsigned visited[256], visited_count;
    SmokeFrame parent = snapshot(), current;
    SmokeItem tested[MAX_ITEMS];
    unsigned hash = screen_key(&parent), j, tested_count = 0;
    int i;
    if (!record_screen(name, depth) || !shot(name)) return 0;
    for (j = 0; j < visited_count; j++) if (visited[j] == hash) return 1;
    if (visited_count >= 256) return fail("screen limit");
    visited[visited_count++] = hash;
    if (depth >= EXPLORE_DEPTH) return 1;
    while (tested_count < MAX_ITEMS) {
        SmokeItem chosen;
        const SmokeItem *item = NULL;
        char point[96], label[64];
        current = snapshot();
        for (i = 0; i < current.count; i++) {
            const SmokeItem *candidate = &current.items[i];
            if (candidate->hidden || candidate->disabled || candidate->width <= 0 || candidate->height <= 0) continue;
            for (j = 0; j < tested_count; j++)
                if (tested[j].kind == candidate->kind && !strcmp(tested[j].text, candidate->text)) break;
            if (j == tested_count) { item = candidate; break; }
        }
        if (!item) return 1;
        chosen = *item; item = &chosen;
        tested[tested_count++] = *item;
        slug(label, sizeof label, item->text);
        snprintf(point, sizeof point, "d%u-%u-%s", depth + 1, tested_count, label);
        if (forbidden_button(item->text) || (join_screen(&parent) && !strcmp(item->text, "Join"))) {
            fprintf(stderr, "SMOKE SKIP %s: exit, persistence or external action\n", item->text);
            continue;
        }
        if ((item->kind == 1 || item->kind == 2) && !item->password) {
            if (!text_test(item, point)) return 0;
            continue;
        }
        if (!click(item->text) || !wait_screen(NULL, 3, 100, FRAME_MS, "explore child")) return 0;
        if (!item->text[0] && item_index(&parent, "Voice Chat") >= 0 && !key_press(KEY_ESCAPE)) return 0;
        current = snapshot();
        if (!record_screen(point, depth + 1) || !shot(point)) return 0;
        if (item->kind == 0 && screen_key(&current) != hash) {
            if (game_screen(&current)) {
                if (!leave_game(NULL)) return 0;
            } else {
                if (!explore(depth + 1, point)) return 0;
                current = snapshot();
                if (item_index(&current, "Back") >= 0) {
                    if (!click("Back")) return 0;
                } else if (!key_press(KEY_ESCAPE)) return 0;
            }
            current = snapshot();
            if (host_screen(&parent) && main_screen(&current)) {
                if (!click("Start") || !wait_screen(host_screen, 3, 100, STEP_MS, "restore host menu")) return 0;
            }
            if (!wait_screen(NULL, 3, 100, FRAME_MS, "explore parent")) return 0;
            current = snapshot();
            if (screen_key(&current) != hash) return fail("Back did not restore parent menu");
        }
    }
    return fail("control limit");
}
static int live_player(const SmokeFrame *view) { return game_screen(view) && view->human >= 0 && view->human < HUMAN_LIMIT; }

static int gun_ready(const SmokeFrame *view) { return live_player(view) && view->weapon >= 0 && view->ammo > 0 && view->weapon_slot == 0 && view->game_state == 2; }

static int fire_button(int down)
{
    SmokeFrame view = snapshot();
    SmokeEvent event = {0};
    event.button.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
    event.button.window = window_id(view.window);
    event.button.button = 1; event.button.state = (uint8_t)down; event.button.clicks = 1;
    InterlockedExchange(&buttons, down);
    return push(&event);
}

static int play(void)
{
    SmokeFrame before, after;
    SmokeEvent motion = {0};
    int i, visual, audio = 0, weapon = 0, ok = 1;
    if (!shot("main") || !click("Start") || !wait_screen(host_screen, 3, 100, STEP_MS, "host menu") || !shot("host")) return 0;
    before = snapshot();
    if (before.net_on && !click("Host")) return 0;
    if (!click("Start") || !wait_screen(game_screen, 3, 0, START_MS, "world load") || !shot("world")) return 0;
    before = snapshot();
    if (before.human < 0) {
        if (item_index(&before, "Enter City") >= 0) { if (!click("Enter City")) return 0; }
        else if (!key_press(KEY_1) || !key_press(KEY_R)) return 0;
    }
    if (!wait_screen(live_player, 3, 0, START_MS, "player spawn") || !shot("standing")) return 0;
    before = snapshot();
    if (!key_hold(KEY_W, 2000)) return 0;
    after = snapshot();
    if (!memcmp(&before.position, &after.position, sizeof before.position)) return fail("W did not move player for two seconds");
    motion.motion.type = EVENT_MOTION; motion.motion.window = window_id(after.window);
    motion.motion.dx = 80; motion.motion.dy = -20;
    InterlockedExchange(&relative_x, 80); InterlockedExchange(&relative_y, -20);
    before = after;
    if (!push(&motion) || !wait_screen(NULL, 3, 0, FRAME_MS, "look")) return 0;
    after = snapshot();
    if (before.yaw == after.yaw) return fail("relative mouse did not turn player view");
    if (!key_press(KEY_SPACE) || !shot("moved")) return 0;
    if (!key_press(KEY_INSERT)) return 0;
    before = snapshot();
    fprintf(stderr, "SMOKE PLAYER money=%d menu=%d weapon=%d ammo=%d\n", before.money, before.player_menu, before.weapon, before.ammo);
    if (before.weapon < 0) {
        if (!key_press(KEY_TAB)) return 0;
        before = snapshot();
        if (item_index(&before, "Weapons") < 0) { shot("weapon-unavailable"); record_screen("weapon-unavailable", 0); return fail("weapon shop unavailable after Tab"); }
        if (!click("Weapons") || !click("9mm") || !click("Back") || !click("Ammo") ||
            !click("9mm Magazine") || !click("Back") || !click("Done")) return 0;
    }
    before = snapshot();
    if (before.weapon_slot >= 2 && !key_press(KEY_1 + before.weapon_slot - 2)) return 0;
    if (before.weapon_slot == 1 && !key_press(KEY_F)) return 0;
    before = snapshot();
    if (before.ammo == 0 && before.magazine_slot >= 1) {
        int loaded;
        InterlockedExchange(&extra_key, KEY_SHIFT);
        loaded = before.magazine_slot == 1 || key_press(KEY_1 + before.magazine_slot - 2);
        if (loaded) loaded = key_press(KEY_R);
        InterlockedExchange(&extra_key, 0);
        if (!loaded) return 0;
    }
    if (!wait_screen(gun_ready, 6, 1000, START_MS, "loaded weapon and active round")) return 0;
    before = snapshot();
    if (before.weapon < 0) { shot("weapon-unavailable"); return fail("no gun in either hand after purchase or inventory selection"); }
    if (!selected_shot("before-fire") || !selected_shot("after-fire")) return fail("play needs before-fire and after-fire captures");
    if (!shot("before-fire")) return 0;
    before = snapshot();
    InterlockedExchange(&peak_sounds, before.sounds);
    InterlockedExchange(&observe_fire, 1);
    for (i = 0; i < 4; i++) {
        if (!fire_button(1) || !wait_screen(NULL, 3, 100, FRAME_MS, "fire held")) return 0;
        if (!i && !shot("after-fire")) ok = 0;
        after = snapshot();
        if (after.sound_calls > before.sound_calls || after.sounds > before.sounds) audio = 1;
        if (after.weapon == before.weapon && (after.ammo < before.ammo || after.cooldown != before.cooldown || after.trigger != before.trigger)) weapon = 1;
        fprintf(stderr, "SMOKE FIRE %d sounds=%d calls=%ld weapon=%d ammo=%d cooldown=%d trigger=%d\n",
                i, after.sounds, after.sound_calls, after.weapon, after.ammo, after.cooldown, after.trigger);
        if (!fire_button(0) || !wait_screen(NULL, 3, 150, FRAME_MS, "fire release")) return 0;
    }
    InterlockedExchange(&observe_fire, 0);
    if (InterlockedCompareExchange(&peak_sounds, 0, 0) > before.sounds) audio = 1;
    EnterCriticalSection(&lock); visual = shot_changed; LeaveCriticalSection(&lock);
    fprintf(stderr, "SMOKE ASSERT fire-image %s\n", visual ? "PASS" : "FAIL");
    fprintf(stderr, "SMOKE ASSERT fire-sound %s before=%d/%ld after=%d/%ld peak=%ld\n", audio ? "PASS" : "FAIL",
            before.sounds, before.sound_calls, after.sounds, after.sound_calls, InterlockedCompareExchange(&peak_sounds, 0, 0));
    fprintf(stderr, "SMOKE ASSERT fire-weapon %s before=%d after=%d\n", weapon ? "PASS" : "FAIL", before.ammo, after.ammo);
    if (!visual) { fail("image did not change after fire"); ok = 0; }
    if (!audio) { fail("no sound count or createsound increase after fire"); ok = 0; }
    if (!weapon) { fail("ammo and gun state did not change after fire"); ok = 0; }
    return shot("after-burst") && ok;
}