/* Frame capture and state samples run on the rendering thread. */
static void json_string(FILE *file, const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    fputc('"', file);
    for (; *p; p++) {
        if (*p == '"' || *p == '\\') fputc('\\', file);
        if (*p < 32 || *p >= 127) fprintf(file, "\\u%04x", *p);
        else fputc(*p, file);
    }
    fputc('"', file);
}

static void slug(char *to, size_t capacity, const char *from)
{
    size_t i = 0;
    while (*from && i + 1 < capacity) {
        unsigned char c = (unsigned char)*from++;
        to[i++] = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ? c :
                  c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : '-';
    }
    to[i] = 0;
}

static void sample_player(void)
{
    frame.sounds = cl_data_audio_sound_count;
    frame.game_state = sr_data_game.state;
    frame.weapon_slot = frame.magazine_slot = -1;
    if (InterlockedCompareExchange(&observe_fire, 0, 0) && frame.sounds > InterlockedCompareExchange(&peak_sounds, 0, 0))
        InterlockedExchange(&peak_sounds, frame.sounds);
    frame.sound_calls = InterlockedCompareExchange(&sound_calls, 0, 0);
    frame.human = frame.weapon = frame.ammo = -1;
    frame.money = frame.player_menu = frame.cooldown = frame.trigger = 0;
    frame.net_on = sr_data_option.net_on;
    if (sr_data_view >= 0 && sr_data_view < PLAYER_LIMIT) {
        const Player *player = &sr_data_player[sr_data_view];
        frame.human = player->human_id;
        frame.money = player->money; frame.player_menu = player->menu; frame.yaw = player->yaw;
        if (frame.human >= 0 && frame.human < HUMAN_LIMIT) {
            const Human *human = &sr_data_human[frame.human];
            int slot, child;
            frame.position = human->bones[0].pos;
            for (slot = 0; slot < 7; slot++) if (human->inventory[slot].count > 0) {
                int id = human->inventory[slot].items[0];
                const Item *item;
                if (id < 0 || id >= ITEM_LIMIT) continue;
                item = &sr_data_item[id];
                if (item->type < 0 || item->type >= 64 || !sr_data_itemtype[item->type].is_gun) continue;
                frame.weapon = id; frame.weapon_slot = slot; frame.ammo = item->ammo;
                frame.cooldown = item->cooldown; frame.trigger = item->trigger_ticks;
                for (child = 0; child < item->num_children && child < 8; child++) {
                    int magazine = item->children[child];
                    if (magazine >= 0 && magazine < ITEM_LIMIT) frame.ammo += sr_data_item[magazine].ammo;
                }
                break;
            }
            if (frame.weapon >= 0) for (slot = 1; slot < 7; slot++) {
                const InventorySlot *inventory = &human->inventory[slot];
                if (inventory->count > 0) {
                    int id = inventory->items[0];
                    if (id >= 0 && id < ITEM_LIMIT && sr_data_item[id].type >= 0 && sr_data_item[id].type < 64 &&
                        sr_data_itemtype[sr_data_item[id].type].fits_in[sr_data_item[frame.weapon].type]) {
                        frame.magazine_slot = slot;
                        break;
                    }
                }
            }
        }
    }
}

/* Write RGB rows as a 24 bit BMP. Each row starts on a four byte boundary. */
static int write_bitmap(const char *path, unsigned char *rgb, int width, int height)
{
    unsigned char header[54] = {0}, padding[3] = {0};
    uint32_t stride = ((uint32_t)width * 3 + 3) & ~3u;
    uint32_t size = 54 + stride * height, offset = 54, info = 40, bits = 24, planes = 1;
    FILE *file = fopen(path, "wb");
    int x, y, ok;
    if (!file) return 0;
    header[0] = 'B'; header[1] = 'M';
    memcpy(header + 2, &size, 4); memcpy(header + 10, &offset, 4);
    memcpy(header + 14, &info, 4); memcpy(header + 18, &width, 4); memcpy(header + 22, &height, 4);
    memcpy(header + 26, &planes, 2); memcpy(header + 28, &bits, 2);
    ok = fwrite(header, 1, sizeof header, file) == sizeof header;
    for (y = 0; ok && y < height; y++) {
        unsigned char *row = rgb + (size_t)y * width * 3;
        for (x = 0; x < width; x++) {
            unsigned char temp = row[x * 3]; row[x * 3] = row[x * 3 + 2]; row[x * 3 + 2] = temp;
        }
        ok = fwrite(row, 3, width, file) == (size_t)width &&
             fwrite(padding, 1, stride - width * 3, file) == stride - width * 3;
    }
    if (fclose(file)) ok = 0;
    return ok;
}

static int capture_buffer(void *window)
{
    int width = 0, height = 0, read_buffer, alignment, valid, ok;
    unsigned char *rgb, *colors;
    size_t pixels, bytes, i, black = 0, distinct = 0, different = 0;
    uint64_t sums[3] = {0, 0, 0};
    char path[MAX_PATH], relative[160];
    FILE *manifest;
    drawable_size(window, &width, &height);
    if (width <= 0 || height <= 0 || width > MAX_DIMENSION || height > MAX_DIMENSION)
        return fail("invalid drawable size");
    pixels = (size_t)width * height; bytes = pixels * 3;
    rgb = (unsigned char *)calloc(bytes, 1); colors = (unsigned char *)calloc(COLOR_BYTES, 1);
    if (!rgb || !colors) { free(rgb); free(colors); return fail("capture allocation"); }
    glGetIntegerv(GL_READ_BUFFER, &read_buffer); glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glPixelStorei(GL_PACK_ALIGNMENT, alignment); glReadBuffer(read_buffer);
    for (i = 0; i < pixels; i++) {
        unsigned char *p = rgb + i * 3;
        unsigned color = (unsigned)p[0] << 16 | (unsigned)p[1] << 8 | p[2];
        unsigned bit = 1u << (color & 7);
        if (!color) black++;
        if (!(colors[color >> 3] & bit)) { colors[color >> 3] |= bit; distinct++; }
        sums[0] += p[0]; sums[1] += p[1]; sums[2] += p[2];
        if (previous_size == bytes && memcmp(previous_pixels + i * 3, p, 3)) different++;
    }
    free(colors);
    shot_changed = previous_size == bytes && different > 0;
    free(previous_pixels); previous_pixels = (unsigned char *)malloc(bytes); previous_size = 0;
    if (previous_pixels) { memcpy(previous_pixels, rgb, bytes); previous_size = bytes; }
    snprintf(relative, sizeof relative, "shots/%02u-%s.bmp", ++shot_count, shot_name);
    snprintf(path, sizeof path, "%s/%s", output_dir, relative);
    ok = write_bitmap(path, rgb, width, height);
    free(rgb);
    valid = black < pixels && distinct > 1;
    snprintf(path, sizeof path, "%s/shots.jsonl", output_dir);
    manifest = fopen(path, "a");
    if (!manifest) return fail("capture manifest open");
    fputs("{\"name\":", manifest); json_string(manifest, shot_name);
    fputs(",\"path\":", manifest); json_string(manifest, relative);
    fprintf(manifest, ",\"width\":%d,\"height\":%d,\"black_fraction\":%.9f,\"distinct_colors\":%u,"
            "\"mean_rgb\":[%.6f,%.6f,%.6f],\"changed_percent\":%.6f,\"ok\":%s}\n",
            width, height, (double)black / pixels, (unsigned)distinct,
            (double)sums[0] / pixels, (double)sums[1] / pixels, (double)sums[2] / pixels,
            100.0 * different / pixels, ok && valid ? "true" : "false");
    if (fclose(manifest)) ok = 0;
    fprintf(stderr, "SMOKE SHOT %s colors=%u black=%.6f changed=%.6f%%\n",
            relative, (unsigned)distinct, (double)black / pixels, 100.0 * different / pixels);
    if (!ok) return fail("capture file write");
    return valid || fail("capture is all black or one color");
}

static unsigned __cdecl smoke_relative(int *x, int *y)
{
    relative_state(x, y);
    if (x) *x = (int)InterlockedExchange(&relative_x, 0);
    if (y) *y = (int)InterlockedExchange(&relative_y, 0);
    return (unsigned)InterlockedCompareExchange(&buttons, 0, 0);
}