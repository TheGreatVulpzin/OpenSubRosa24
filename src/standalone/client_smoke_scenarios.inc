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
    if (!shot("round-before-team") || !record_screen("round-before-team", 0)) return 0;
    fprintf(stderr, "SMOKE CAMERA pos=%f %f %f free=%f %f %f yaw=%f pitch=%f zoom=%f\n",
            cl_data_audio_camera.pos.x, cl_data_audio_camera.pos.y, cl_data_audio_camera.pos.z,
            ((InputCamera *)sr_data_camera)[0].pos.x, ((InputCamera *)sr_data_camera)[0].pos.y, ((InputCamera *)sr_data_camera)[0].pos.z,
            ((InputCamera *)sr_data_camera)[0].yaw, ((InputCamera *)sr_data_camera)[0].pitch, cl_data_audio_camera.projection);
    before = snapshot();
    if (before.human < 0) {
        if (item_index(&before, "Enter City") >= 0) { if (!click("Enter City")) return 0; }
        else if (!key_press(KEY_1) || !shot("round-after-team") || !record_screen("round-after-team", 0) || !key_press(KEY_R)) return 0;
    }
    if (getenv("SR_SMOKE_ROUND_ONLY")) return 1;
    before = snapshot();
    fprintf(stderr, "SMOKE ASSERT round-team %s team=%d\n", before.team == 0 ? "PASS" : "FAIL", before.team);
    if (before.team != 0) return fail("team key did not select first team");
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
        if (!shot("shop") || !record_screen("shop", 0)) return 0;
        if (!click("Weapons") || !shot("weapon-shop") || !click("9mm") || !click("Back") || !click("Ammo") ||
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
    fprintf(stderr, "SMOKE VOICE before enabled=%d push=%d key=%d mouse=%d off=%d head=%d capture=%u\n",
            before.voice_enabled, before.voice_push, before.voice_key, before.voice_mouse,
            before.voice_off, before.voice_head, capture_count);
    if (!before.voice_enabled || !before.voice_push || before.voice_key != 25 || before.voice_mouse != 0)
        { fail("push to talk settings differ from fixture"); ok = 0; }
    if (!capture_count || !before.voice_off) return fail("microphone is not idle before fire");
    InterlockedExchange(&observe_fire, 1);
    for (i = 0; i < 4; i++) {
        if (!fire_button(1) || !wait_screen(NULL, 3, 100, FRAME_MS, "fire held")) return 0;
        if (!i && !shot("after-fire")) ok = 0;
        after = snapshot();
        fprintf(stderr, "SMOKE VOICE fire=%d off=%d head=%d capture=%u\n", i, after.voice_off, after.voice_head, capture_count);
        if (!after.voice_off) { fail("left fire button activates microphone"); ok = 0; }
        if (after.sound_calls > before.sound_calls || after.sounds > before.sounds) audio = 1;
        if (after.weapon == before.weapon && (after.ammo < before.ammo || after.cooldown != before.cooldown || after.trigger != before.trigger)) weapon = 1;
        fprintf(stderr, "SMOKE FIRE %d sounds=%d calls=%ld weapon=%d ammo=%d cooldown=%d trigger=%d\n",
                i, after.sounds, after.sound_calls, after.weapon, after.ammo, after.cooldown, after.trigger);
        if (!fire_button(0) || !wait_screen(NULL, 3, 150, FRAME_MS, "fire release")) return 0;
    }
    InterlockedExchange(&observe_fire, 0);
    if (InterlockedCompareExchange(&peak_sounds, 0, 0) > before.sounds) audio = 1;
    EnterCriticalSection(&lock); visual = shot_changed; LeaveCriticalSection(&lock);
    fprintf(stderr, "SMOKE ASSERT fire-voice %s\n", ok ? "PASS" : "FAIL");
    fprintf(stderr, "SMOKE ASSERT fire-image %s\n", visual ? "PASS" : "FAIL");
    fprintf(stderr, "SMOKE ASSERT fire-sound %s before=%d/%ld after=%d/%ld peak=%ld\n", audio ? "PASS" : "FAIL",
            before.sounds, before.sound_calls, after.sounds, after.sound_calls, InterlockedCompareExchange(&peak_sounds, 0, 0));
    fprintf(stderr, "SMOKE ASSERT fire-weapon %s before=%d after=%d\n", weapon ? "PASS" : "FAIL", before.ammo, after.ammo);
    if (!visual) { fail("image did not change after fire"); ok = 0; }
    if (!audio) { fail("no sound count or createsound increase after fire"); ok = 0; }
    if (!weapon) { fail("ammo and gun state did not change after fire"); ok = 0; }
    if (!shot("after-burst") || !key_hold(25, 300)) return 0;
    after = snapshot();
    fprintf(stderr, "SMOKE ASSERT voice-key %s off=%d\n", !after.voice_off ? "PASS" : "FAIL", after.voice_off);
    if (after.voice_off) { fail("voice key did not activate microphone"); ok = 0; }
    if (!wait_screen(NULL, 30, 1000, FRAME_MS, "voice key release")) return 0;
    after = snapshot();
    fprintf(stderr, "SMOKE ASSERT voice-release %s off=%d\n", after.voice_off ? "PASS" : "FAIL", after.voice_off);
    if (!after.voice_off) { fail("microphone did not stop after voice key release"); ok = 0; }
    return ok;
}