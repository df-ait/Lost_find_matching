#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "item_persist.h"
#include "item_mgr.h"
#include "ui_util.h"

#define LINE_BUF 4096
#define TIME_FMT "%Y-%m-%d %H:%M"

static int ensure_data_dir(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *sep = slash > bslash ? slash : bslash;
    if (!sep) return 0;

    char dir[512];
    size_t len = (size_t)(sep - path);
    if (len >= sizeof(dir)) return -1;
    memcpy(dir, path, len);
    dir[len] = '\0';

#ifdef _WIN32
    return _mkdir(dir);
#else
    return mkdir(dir, 0755);
#endif
}

static void write_escaped(FILE *f, const char *s)
{
    if(!s) return;
    for(; *s; s++){
        if(*s == '\\')
            fputs("\\\\", f);
        else if(*s == '\n')
            fputs("\\n", f);
        else if(*s == '\r')
            continue;
        else
            fputc(*s, f);
    }
}

static void read_escaped(const char *src, char *dst, size_t dst_len)
{
    if (!dst || dst_len == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dst_len; i++) {
        if (src[i] == '\\' && src[i + 1]) {
            if (src[i + 1] == 'n') {
                dst[j++] = '\n';
                i++;
            } else if (src[i + 1] == '\\') {
                dst[j++] = '\\';
                i++;
            } else {
                dst[j++] = src[i];
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static int item_id_num(const char *id)
{
    if (!id || strncmp(id, "LF", 2) != 0) return 0;
    return atoi(id + 2);
}

static void trim_line(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' '))
        s[--n] = '\0';
}

static void write_time_field(FILE *f, const char *key, time_t t)
{
    if (t <= 0) {
        fprintf(f, "%s=\n", key);
        return;
    }
    char buf[32];
    struct tm *tm_info = localtime(&t);
    strftime(buf, sizeof(buf), TIME_FMT, tm_info);
    fprintf(f, "%s=%s\n", key, buf);
}

/* 兼容旧版 Unix 秒数 与 新版 YYYY-MM-DD HH:MM */
static int parse_time_field(const char *val, time_t *out)
{
    if (!out) return -1;
    if (!val || !val[0]) {
        *out = 0;
        return 0;
    }
    if (strchr(val, '-')) {
        if (ui_parse_datetime(val, out) == 0)
            return 0;
        return -1;
    }
    char *end = NULL;
    long long sec = strtoll(val, &end, 10);
    if (end && *end == '\0' && sec > 0) {
        *out = (time_t)sec;
        return 0;
    }
    return -1;
}

static int write_one_item(FILE *f, const Item *item)
{
    fprintf(f, "[ITEM]\n");
    fprintf(f, "itemId="); write_escaped(f, item->itemId); fputc('\n', f);
    fprintf(f, "name="); write_escaped(f, item->name); fputc('\n', f);
    fprintf(f, "category="); write_escaped(f, item->category); fputc('\n', f);
    write_time_field(f, "foundTime", item->foundTime);
    fprintf(f, "location="); write_escaped(f, item->location); fputc('\n', f);
    fprintf(f, "description="); write_escaped(f, item->description); fputc('\n', f);
    fprintf(f, "secretFeatures="); write_escaped(f, item->secretFeatures); fputc('\n', f);
    fprintf(f, "has_secret=%d\n", item->has_secret);
    fprintf(f, "status=%d\n", (int)item->status);
    write_time_field(f, "registerTime", item->registerTime);
    fprintf(f, "claimerName="); write_escaped(f, item->claimerName); fputc('\n', f);
    fprintf(f, "claimerPhone="); write_escaped(f, item->claimerPhone); fputc('\n', f);
    write_time_field(f, "claimTime", item->claimTime);
    fprintf(f, "[/ITEM]\n");
    return 0;
}

int store_save_items(const ItemStore *store, const char *path)
{
    if (!store || !path || !path[0]) return -1;

    ensure_data_dir(path);

    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *f = fopen(tmp, "w");
    if (!f) return -1;

    fprintf(f, "# lost_find items v2\n");
    fprintf(f, "id_counter=%d\n\n", store->id_counter);

    for (ListNode *p = store->all_items->head; p; p = p->next)
        write_one_item(f, &p->item);

    fclose(f);

    remove(path);
    if (rename(tmp, path) != 0) {
        FILE *in = fopen(tmp, "r");
        FILE *out = fopen(path, "w");
        if (in && out) {
            char buf[LINE_BUF];
            while (fgets(buf, sizeof(buf), in))
                fputs(buf, out);
        }
        if (in) fclose(in);
        if (out) fclose(out);
        remove(tmp);
    }
    return 0;
}

static void apply_field(Item *item, const char *key, const char *raw_val)
{
    char val[512];
    read_escaped(raw_val, val, sizeof(val));

    if (strcmp(key, "itemId") == 0)
        strncpy(item->itemId, val, ID_LEN - 1);
    else if (strcmp(key, "name") == 0)
        strncpy(item->name, val, NAME_LEN - 1);
    else if (strcmp(key, "category") == 0)
        strncpy(item->category, val, CATEGORY_LEN - 1);
    else if (strcmp(key, "foundTime") == 0) {
        time_t t;
        if (parse_time_field(val, &t) == 0) item->foundTime = t;
    } else if (strcmp(key, "location") == 0)
        strncpy(item->location, val, LOC_LEN - 1);
    else if (strcmp(key, "description") == 0)
        strncpy(item->description, val, DESC_LEN - 1);
    else if (strcmp(key, "secretFeatures") == 0)
        strncpy(item->secretFeatures, val, SECRET_LEN - 1);
    else if (strcmp(key, "has_secret") == 0)
        item->has_secret = atoi(val);
    else if (strcmp(key, "status") == 0)
        item->status = (ItemStatus)atoi(val);
    else if (strcmp(key, "registerTime") == 0) {
        time_t t;
        if (parse_time_field(val, &t) == 0) item->registerTime = t;
    } else if (strcmp(key, "claimerName") == 0)
        strncpy(item->claimerName, val, NAME_LEN - 1);
    else if (strcmp(key, "claimerPhone") == 0)
        strncpy(item->claimerPhone, val, PHONE_LEN - 1);
    else if (strcmp(key, "claimTime") == 0) {
        time_t t;
        if (parse_time_field(val, &t) == 0) item->claimTime = t;
    }
}

static int flush_item(ItemStore *store, Item *item, int *max_id)
{
    if (!item->itemId[0]) return 0;

    if (item->secretFeatures[0] && !item->has_secret)
        item_bind_secret(item);

    int n = item_id_num(item->itemId);
    if (n > *max_id) *max_id = n;

    return store_register_item(store, item);
}

int store_load_items(ItemStore *store, const char *path)
{
    if (!store || !path || !path[0]) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[LINE_BUF];
    int file_counter = 0;
    int max_id = 0;
    int in_item = 0;
    Item cur;
    int rc = 0;

    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        if (!line[0] || line[0] == '#') continue;

        if (strncmp(line, "id_counter=", 11) == 0) {
            file_counter = atoi(line + 11);
            continue;
        }

        if (strcmp(line, "[ITEM]") == 0) {
            memset(&cur, 0, sizeof(cur));
            cur.status = ITEM_IN_STORAGE;
            in_item = 1;
            continue;
        }

        if (strcmp(line, "[/ITEM]") == 0) {
            if (in_item && flush_item(store, &cur, &max_id) != 0)
                rc = -1;
            in_item = 0;
            continue;
        }

        if (!in_item) continue;

        const char *eq = strchr(line, '=');
        if (!eq) continue;

        char key[64];
        size_t klen = (size_t)(eq - line);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, line, klen);
        key[klen] = '\0';
        apply_field(&cur, key, eq + 1);
    }

    if (in_item && flush_item(store, &cur, &max_id) != 0)
        rc = -1;

    fclose(f);

    if (file_counter > max_id)
        max_id = file_counter;
    if (max_id > store->id_counter)
        store->id_counter = max_id;

    return rc;
}
