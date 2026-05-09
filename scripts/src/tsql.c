#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define DEFAULT_URL "https://qlfinntromtdvjxyvbyn.supabase.co"
#define DEFAULT_KEY "sb_publishable_jEf_67lB3bvHpFVGL8ov_Q_Xi3iV1il"

#define MAX_SQL 8192
#define MAX_URL 16384
#define MAX_BODY 16384

typedef enum {
    STMT_SELECT,
    STMT_INSERT,
    STMT_UPDATE,
    STMT_DELETE,
    STMT_UNKNOWN
} StmtType;

typedef struct {
    StmtType type;
    char table[128];
    char select_cols[1024];
    char where[2048];
    char order_col[128];
    char order_dir[16];
    int limit;
    int offset;
    char insert_cols[1024];
    char insert_vals[4096];
    char assignments[4096];
} Query;

static void trim_inplace(char *s) {
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    if (n > 0 && s[n - 1] == ';') s[--n] = '\0';
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static void trim_spaces_inplace(char *s) {
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static void strip_quotes(char *s) {
    trim_inplace(s);
    size_t n = strlen(s);
    if (n >= 2 && ((s[0] == '\'' && s[n - 1] == '\'') || (s[0] == '"' && s[n - 1] == '"'))) {
        memmove(s, s + 1, n - 2);
        s[n - 2] = '\0';
    }
}

static int ci_char(int c) { return tolower((unsigned char)c); }

static char *ci_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (const char *h = haystack; *h; h++) {
        const char *a = h;
        const char *b = needle;
        while (*a && *b && ci_char(*a) == ci_char(*b)) {
            a++;
            b++;
        }
        if (!*b) return (char *)h;
    }
    return NULL;
}

static bool starts_with_ci(const char *s, const char *prefix) {
    while (*prefix) {
        if (ci_char(*s++) != ci_char(*prefix++)) return false;
    }
    return true;
}

static bool is_identifier(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '.')) return false;
    }
    return true;
}

static bool parse_nonnegative_int(const char *s, int *out) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s", s ? s : "");
    trim_inplace(tmp);
    if (!tmp[0]) return false;
    for (char *p = tmp; *p; p++) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    *out = atoi(tmp);
    return true;
}

static bool contains_unsupported_boolean(const char *s) {
    return ci_strstr(s, " or ") || strchr(s, '(') || strchr(s, ')');
}

static char *url_encode(const char *s) {
    const char *hex = "0123456789ABCDEF";
    size_t len = strlen(s ? s : "");
    char *out = malloc(len * 3 + 1);
    if (!out) return NULL;
    char *w = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '*') {
            *w++ = (char)c;
        } else {
            *w++ = '%';
            *w++ = hex[c >> 4];
            *w++ = hex[c & 15];
        }
    }
    *w = '\0';
    return out;
}

static void append_param(char *url, const char *key, const char *value, bool *first) {
    char *enc = url_encode(value);
    strcat(url, *first ? "?" : "&");
    strcat(url, key);
    strcat(url, "=");
    strcat(url, enc);
    *first = false;
    free(enc);
}

static void split_once_ci(char *src, const char *needle, char *left, size_t left_sz, char *right, size_t right_sz) {
    char *p = ci_strstr(src, needle);
    if (!p) {
        snprintf(left, left_sz, "%s", src);
        right[0] = '\0';
        return;
    }
    *p = '\0';
    snprintf(left, left_sz, "%s", src);
    snprintf(right, right_sz, "%s", p + strlen(needle));
    trim_inplace(left);
    trim_inplace(right);
}

static bool parse_tail(char *tail, Query *q) {
    char work[4096];
    snprintf(work, sizeof(work), "%s", tail);
    char *p;
    if ((p = ci_strstr(work, " limit "))) {
        if (!parse_nonnegative_int(p + 7, &q->limit)) return false;
        *p = '\0';
    }
    if ((p = ci_strstr(work, " offset "))) {
        if (!parse_nonnegative_int(p + 8, &q->offset)) return false;
        *p = '\0';
    }
    if ((p = ci_strstr(work, " order by "))) {
        char order[256];
        snprintf(order, sizeof(order), "%s", p + 10);
        *p = '\0';
        trim_inplace(order);
        char extra[64] = "";
        if (sscanf(order, "%127s %15s %63s", q->order_col, q->order_dir, extra) < 1) return false;
        if (extra[0]) return false;
        if (!is_identifier(q->order_col)) return false;
        if (!q->order_dir[0]) snprintf(q->order_dir, sizeof(q->order_dir), "asc");
        if (strcasecmp(q->order_dir, "asc") != 0 && strcasecmp(q->order_dir, "desc") != 0) return false;
    }
    if ((p = ci_strstr(work, " where "))) {
        snprintf(q->where, sizeof(q->where), "%s", p + 7);
        *p = '\0';
        if (contains_unsupported_boolean(q->where)) return false;
    }
    trim_inplace(work);
    char extra[128] = "";
    if (!q->table[0] && sscanf(work, "%127s %127s", q->table, extra) < 1) return false;
    if (extra[0]) return false;
    if (!is_identifier(q->table)) return false;
    trim_inplace(q->where);
    return true;
}

static bool parse_select(char *sql, Query *q) {
    q->type = STMT_SELECT;
    char *from = ci_strstr(sql, " from ");
    if (!from) return false;
    *from = '\0';
    snprintf(q->select_cols, sizeof(q->select_cols), "%s", sql + 6);
    trim_inplace(q->select_cols);
    if (!q->select_cols[0]) return false;
    char tail[4096];
    snprintf(tail, sizeof(tail), "%s", from + 6);
    if (!parse_tail(tail, q)) return false;
    return q->table[0] != '\0';
}

static bool parse_delete(char *sql, Query *q) {
    q->type = STMT_DELETE;
    char *from = ci_strstr(sql, "from ");
    if (!from) return false;
    char tail[4096];
    snprintf(tail, sizeof(tail), "%s", from + 5);
    if (!parse_tail(tail, q)) return false;
    return q->table[0] != '\0';
}

static bool parse_update(char *sql, Query *q) {
    q->type = STMT_UPDATE;
    char *set = ci_strstr(sql, " set ");
    if (!set) return false;
    *set = '\0';
    char extra[128] = "";
    if (sscanf(sql + 6, "%127s %127s", q->table, extra) != 1) return false;
    if (!is_identifier(q->table)) return false;
    char rest[4096];
    snprintf(rest, sizeof(rest), "%s", set + 5);
    split_once_ci(rest, " where ", q->assignments, sizeof(q->assignments), q->where, sizeof(q->where));
    if (contains_unsupported_boolean(q->where)) return false;
    return q->table[0] && q->assignments[0];
}

static bool parse_insert(char *sql, Query *q) {
    q->type = STMT_INSERT;
    char *into = ci_strstr(sql, "into ");
    char *values = ci_strstr(sql, " values ");
    if (!into || !values) return false;
    *values = '\0';
    char head[2048];
    snprintf(head, sizeof(head), "%s", into + 5);
    trim_inplace(head);
    char *lp = strchr(head, '(');
    char *rp = strrchr(head, ')');
    if (!lp || !rp || rp <= lp) return false;
    *lp = '\0';
    *rp = '\0';
    trim_inplace(head);
    if (!is_identifier(head)) return false;
    snprintf(q->table, sizeof(q->table), "%s", head);
    snprintf(q->insert_cols, sizeof(q->insert_cols), "%s", lp + 1);
    char vals[4096];
    snprintf(vals, sizeof(vals), "%s", values + 8);
    trim_inplace(vals);
    lp = strchr(vals, '(');
    rp = strrchr(vals, ')');
    if (!lp || !rp || rp <= lp) return false;
    *rp = '\0';
    snprintf(q->insert_vals, sizeof(q->insert_vals), "%s", lp + 1);
    return q->table[0] && q->insert_cols[0] && q->insert_vals[0];
}

static bool parse_sql(const char *input, Query *q) {
    memset(q, 0, sizeof(*q));
    q->type = STMT_UNKNOWN;
    q->limit = -1;
    q->offset = -1;
    char sql[MAX_SQL];
    snprintf(sql, sizeof(sql), "%s", input);
    trim_inplace(sql);
    if (starts_with_ci(sql, "select ")) return parse_select(sql, q);
    if (starts_with_ci(sql, "insert ")) return parse_insert(sql, q);
    if (starts_with_ci(sql, "update ")) return parse_update(sql, q);
    if (starts_with_ci(sql, "delete ")) return parse_delete(sql, q);
    return false;
}

static void value_to_json(char *dst, size_t dst_sz, const char *value) {
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", value);
    strip_quotes(tmp);
    bool numeric = tmp[0] && (isdigit((unsigned char)tmp[0]) || tmp[0] == '-');
    for (size_t i = numeric ? 1 : 0; tmp[i]; i++) {
        if (!isdigit((unsigned char)tmp[i]) && tmp[i] != '.') numeric = false;
    }
    if (strcasecmp(tmp, "null") == 0) snprintf(dst, dst_sz, "null");
    else if (strcasecmp(tmp, "true") == 0 || strcasecmp(tmp, "false") == 0) snprintf(dst, dst_sz, "%s", tmp);
    else if (numeric) snprintf(dst, dst_sz, "%s", tmp);
    else {
        char out[2048] = "\"";
        for (size_t i = 0; tmp[i] && strlen(out) + 4 < sizeof(out); i++) {
            if (tmp[i] == '"' || tmp[i] == '\\') strcat(out, "\\");
            char one[2] = {tmp[i], 0};
            strcat(out, one);
        }
        strcat(out, "\"");
        snprintf(dst, dst_sz, "%s", out);
    }
}

static void build_json_from_pairs(char *dst, size_t dst_sz, const char *cols, const char *vals, bool assignments) {
    char cbuf[4096], vbuf[4096];
    snprintf(cbuf, sizeof(cbuf), "%s", cols);
    snprintf(vbuf, sizeof(vbuf), "%s", vals);
    dst[0] = '\0';
    strcat(dst, "{");
    char *save1 = NULL, *save2 = NULL;
    char *c = strtok_r(cbuf, ",", &save1);
    char *v = assignments ? NULL : strtok_r(vbuf, ",", &save2);
    bool first = true;
    while (c) {
        char key[256] = "";
        char val[2048] = "";
        if (assignments) {
            char *eq = strchr(c, '=');
            if (!eq) break;
            *eq = '\0';
            snprintf(key, sizeof(key), "%s", c);
            snprintf(val, sizeof(val), "%s", eq + 1);
        } else {
            if (!v) break;
            snprintf(key, sizeof(key), "%s", c);
            snprintf(val, sizeof(val), "%s", v);
        }
        trim_inplace(key);
        strip_quotes(key);
        char json_val[4096];
        value_to_json(json_val, sizeof(json_val), val);
        if (!first) strcat(dst, ",");
        first = false;
        strcat(dst, "\"");
        strcat(dst, key);
        strcat(dst, "\":");
        strcat(dst, json_val);
        c = strtok_r(NULL, ",", &save1);
        if (!assignments) v = strtok_r(NULL, ",", &save2);
    }
    strcat(dst, "}");
}

static bool add_where_params(char *url, const char *where, bool *first) {
    if (!where || !where[0]) return true;
    char w[4096];
    snprintf(w, sizeof(w), "%s", where);
    char *part = w;
    while (part && *part) {
        char *next = ci_strstr(part, " and ");
        if (next) {
            *next = '\0';
            next += 5;
        }
        trim_inplace(part);
        char col[128] = "", op[16] = "", val[1024] = "";
        if (sscanf(part, "%127s %15s %1023[^\n]", col, op, val) >= 3) {
            strip_quotes(val);
            char expr[1200];
            if (strcasecmp(op, "=") == 0) snprintf(expr, sizeof(expr), "eq.%s", val);
            else if (strcasecmp(op, "!=") == 0 || strcasecmp(op, "<>") == 0) snprintf(expr, sizeof(expr), "neq.%s", val);
            else if (strcasecmp(op, ">") == 0) snprintf(expr, sizeof(expr), "gt.%s", val);
            else if (strcasecmp(op, ">=") == 0) snprintf(expr, sizeof(expr), "gte.%s", val);
            else if (strcasecmp(op, "<") == 0) snprintf(expr, sizeof(expr), "lt.%s", val);
            else if (strcasecmp(op, "<=") == 0) snprintf(expr, sizeof(expr), "lte.%s", val);
            else if (strcasecmp(op, "like") == 0) {
                for (char *p = val; *p; p++) if (*p == '%') *p = '*';
                snprintf(expr, sizeof(expr), "like.%s", val);
            } else if (strcasecmp(op, "ilike") == 0) {
                for (char *p = val; *p; p++) if (*p == '%') *p = '*';
                snprintf(expr, sizeof(expr), "ilike.%s", val);
            } else {
                fprintf(stderr, "unsupported where operator: %s\n", op);
                return false;
            }
            append_param(url, col, expr, first);
        } else {
            fprintf(stderr, "invalid where clause: %s\n", part);
            return false;
        }
        part = next;
    }
    return true;
}

static bool build_request(const Query *q, char *method, size_t method_sz, char *url, size_t url_sz, char *body, size_t body_sz) {
    const char *base = getenv("TSQL_SUPABASE_URL");
    if (!base || !*base) base = DEFAULT_URL;
    snprintf(url, url_sz, "%s/rest/v1/%s", base, q->table);
    body[0] = '\0';
    bool first = true;
    if (q->type == STMT_SELECT) {
        snprintf(method, method_sz, "GET");
        append_param(url, "select", q->select_cols[0] ? q->select_cols : "*", &first);
        if (!add_where_params(url, q->where, &first)) return false;
        if (q->order_col[0]) {
            char order[256];
            snprintf(order, sizeof(order), "%s.%s", q->order_col, q->order_dir[0] ? q->order_dir : "asc");
            append_param(url, "order", order, &first);
        }
        if (q->limit >= 0) {
            char n[32]; snprintf(n, sizeof(n), "%d", q->limit); append_param(url, "limit", n, &first);
        }
        if (q->offset >= 0) {
            char n[32]; snprintf(n, sizeof(n), "%d", q->offset); append_param(url, "offset", n, &first);
        }
    } else if (q->type == STMT_INSERT) {
        snprintf(method, method_sz, "POST");
        append_param(url, "select", "*", &first);
        build_json_from_pairs(body, body_sz, q->insert_cols, q->insert_vals, false);
    } else if (q->type == STMT_UPDATE) {
        snprintf(method, method_sz, "PATCH");
        append_param(url, "select", "*", &first);
        if (!add_where_params(url, q->where, &first)) return false;
        build_json_from_pairs(body, body_sz, q->assignments, "", true);
    } else if (q->type == STMT_DELETE) {
        snprintf(method, method_sz, "DELETE");
        append_param(url, "select", "*", &first);
        if (!add_where_params(url, q->where, &first)) return false;
    } else return false;
    return true;
}

static void shell_quote(char *dst, size_t dst_sz, const char *src) {
    dst[0] = '\0';
    strncat(dst, "'", dst_sz - strlen(dst) - 1);
    for (const char *p = src; *p && strlen(dst) + 5 < dst_sz; p++) {
        if (*p == '\'') strncat(dst, "'\\''", dst_sz - strlen(dst) - 1);
        else {
            char one[2] = {*p, 0};
            strncat(dst, one, dst_sz - strlen(dst) - 1);
        }
    }
    strncat(dst, "'", dst_sz - strlen(dst) - 1);
}

static bool is_hex_digest(const char *s) {
    if (!s || strlen(s) < 32) return false;
    for (int i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)s[i])) return false;
    }
    return true;
}

static int print_fingerprint(const char *type, const char *title) {
    if (!type || !*type || !title || !*title) {
        fprintf(stderr, "usage: tsql --fingerprint TYPE TITLE\n");
        return 2;
    }
    for (const char *p = type; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) {
            fprintf(stderr, "invalid type for fingerprint: %s\n", type);
            return 2;
        }
    }
    char q_title[MAX_SQL * 2];
    shell_quote(q_title, sizeof(q_title), title);
    char cmd[MAX_SQL * 2 + 128];
    snprintf(cmd, sizeof(cmd), "printf %%s %s | md5sum", q_title);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("md5sum");
        return 2;
    }
    char digest[128] = "";
    if (!fgets(digest, sizeof(digest), fp)) {
        pclose(fp);
        fprintf(stderr, "failed to generate md5 digest\n");
        return 2;
    }
    int rc = pclose(fp);
    if (rc != 0 || !is_hex_digest(digest)) {
        fprintf(stderr, "failed to generate md5 digest\n");
        return 2;
    }
    digest[16] = '\0';
    printf("%s_%s\n", type, digest);
    return 0;
}

static int http_request(const char *method, const char *url, const char *body, bool count) {
    const char *key = getenv("TSQL_SUPABASE_KEY");
    if (!key || !*key) key = DEFAULT_KEY;
    char q_url[MAX_URL * 2], q_apikey[4096], q_auth[4096], q_prefer[256], q_body[MAX_BODY * 2];
    char apikey[2048], auth[2048], prefer[128];
    snprintf(apikey, sizeof(apikey), "apikey: %s", key);
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", key);
    snprintf(prefer, sizeof(prefer), "%s", count ? "Prefer: count=exact,return=representation" : "Prefer: return=representation");
    shell_quote(q_url, sizeof(q_url), url);
    shell_quote(q_apikey, sizeof(q_apikey), apikey);
    shell_quote(q_auth, sizeof(q_auth), auth);
    shell_quote(q_prefer, sizeof(q_prefer), prefer);
    shell_quote(q_body, sizeof(q_body), body ? body : "");
    char cmd[MAX_URL * 3 + MAX_BODY * 2];
    snprintf(cmd, sizeof(cmd),
             "curl -sS -X %s %s -H %s -H %s -H 'Content-Type: application/json' -H 'Accept: application/json' -H %s%s%s",
             method, q_url, q_apikey, q_auth, q_prefer,
             body && body[0] ? " --data " : "",
             body && body[0] ? q_body : "");
    int rc = system(cmd);
    if (rc != 0) fprintf(stderr, "\nrequest failed with exit code %d\n", rc);
    else printf("\n");
    return rc == 0 ? 0 : 1;
}

static int run_raw_get_path(const char *path) {
    const char *base = getenv("TSQL_SUPABASE_URL");
    if (!base || !*base) base = DEFAULT_URL;
    char url[MAX_URL];
    snprintf(url, sizeof(url), "%s/rest/v1/%s", base, path);
    return http_request("GET", url, "", false);
}

static char *read_stdin_sql(void) {
    char *buf = calloc(1, MAX_SQL);
    if (!buf) return NULL;
    size_t used = 0;
    int c;
    while ((c = getchar()) != EOF && used + 1 < MAX_SQL) buf[used++] = (char)c;
    buf[used] = '\0';
    return buf;
}

static char *read_file_text(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        return NULL;
    }
    char *buf = calloc(1, MAX_SQL);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    size_t n = fread(buf, 1, MAX_SQL - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return buf;
}

static void usage(void) {
    printf("tsql - SQL-subset CLI for Supabase REST\n");
    printf("usage: tsql [--dry-run] [--count] [-c SQL] [-f FILE] [SQL]\n");
    printf("       tsql --fingerprint TYPE TITLE\n");
    printf("       tsql\n");
    printf("examples:\n");
    printf("  tsql -c \"select * from questions limit 5\"\n");
    printf("  tsql --fingerprint 2 \"question title\"\n");
    printf("  tsql -f script.sql\n");
    printf("  tsql \"select * from questions limit 5\"\n");
    printf("  tsql \"select id,title,answer from questions where type = '3' order by id desc limit 10\"\n");
    printf("  tsql \"insert into questions (fingerprint,title,answer,type) values ('x','t','A','3')\"\n");
    printf("  tsql \"update questions set answer = 'B' where id = 81\"\n");
    printf("  tsql \"delete from questions where id = 81\"\n");
}

static int execute_sql(const char *sql, bool dry_run, bool count) {
    Query q;
    if (!parse_sql(sql, &q)) {
        fprintf(stderr, "unsupported or invalid SQL. Supported: SELECT/INSERT/UPDATE/DELETE with simple WHERE/ORDER/LIMIT/OFFSET.\n");
        return 2;
    }
    char method[16], url[MAX_URL], body[MAX_BODY];
    if (!build_request(&q, method, sizeof(method), url, sizeof(url), body, sizeof(body))) {
        fprintf(stderr, "failed to build request\n");
        return 2;
    }
    if (dry_run) {
        printf("method: %s\nurl: %s\n", method, url);
        if (body[0]) printf("body: %s\n", body);
        return 0;
    }
    return http_request(method, url, body, count);
}

static void slash_help(void) {
    printf("Available slash commands:\n");
    printf("  \\?              show this help\n");
    printf("  \\h              show SQL help\n");
    printf("  \\q              quit\n");
    printf("  \\dt             list known tables\n");
    printf("  \\d TABLE        describe a table by fetching one sample row\n");
    printf("  \\conninfo       show current Supabase REST endpoint\n");
}

static void sql_help(void) {
    printf("Supported SQL subset:\n");
    printf("  SELECT columns FROM table [WHERE ...] [ORDER BY col ASC|DESC] [LIMIT n] [OFFSET n]\n");
    printf("  INSERT INTO table (columns) VALUES (values)\n");
    printf("  UPDATE table SET column = value [, ...] WHERE ...\n");
    printf("  DELETE FROM table WHERE ...\n");
    printf("WHERE operators: =, !=, <>, >, >=, <, <=, LIKE, ILIKE\n");
    printf("Fuzzy examples: where title LIKE '%%Amdahl%%', where title ILIKE '%%Amdahl%%'\n");
}

static int handle_slash_command(char *line) {
    if (strcmp(line, "\\q") == 0) return 1;
    if (strcmp(line, "\\?") == 0) {
        slash_help();
        return 0;
    }
    if (strcmp(line, "\\h") == 0) {
        sql_help();
        return 0;
    }
    if (strcmp(line, "\\dt") == 0) {
        printf("Known tables exposed by this Supabase REST project:\n");
        printf("  public.questions\n");
        return 0;
    }
    if (starts_with_ci(line, "\\d ")) {
        char table[128];
        snprintf(table, sizeof(table), "%s", line + 3);
        trim_inplace(table);
        if (!table[0]) {
            fprintf(stderr, "usage: \\d TABLE\n");
            return 0;
        }
        char path[512];
        snprintf(path, sizeof(path), "%s?select=*&limit=1", table);
        printf("Sample row for %s:\n", table);
        run_raw_get_path(path);
        return 0;
    }
    if (strcmp(line, "\\conninfo") == 0) {
        const char *base = getenv("TSQL_SUPABASE_URL");
        if (!base || !*base) base = DEFAULT_URL;
        printf("Connected to Supabase REST endpoint: %s/rest/v1\n", base);
        return 0;
    }
    fprintf(stderr, "unknown slash command: %s\nTry \\? for help.\n", line);
    return 0;
}

static int repl(bool dry_run, bool count) {
    char line[MAX_SQL];
    printf("tsql interactive mode. Type exit, quit, or \\q to quit.\n");
    while (true) {
        printf("tsql=> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            return 0;
        }
        trim_inplace(line);
        if (!line[0]) continue;
        if (strcasecmp(line, "exit") == 0 || strcasecmp(line, "quit") == 0 || strcmp(line, "\\q") == 0) return 0;
        if (line[0] == '\\') {
            if (handle_slash_command(line)) return 0;
            continue;
        }
        execute_sql(line, dry_run, count);
    }
}

static void strip_line_comment(char *line) {
    bool in_single = false;
    bool in_double = false;
    for (char *p = line; *p; p++) {
        if (*p == '\'' && !in_double) in_single = !in_single;
        else if (*p == '"' && !in_single) in_double = !in_double;
        else if (!in_single && !in_double && p[0] == '-' && p[1] == '-') {
            *p = '\0';
            return;
        }
    }
}

static int process_script(char *input, bool dry_run, bool count) {
    char *save = NULL;
    char *line = strtok_r(input, "\n", &save);
    int rc = 0;
    char stmt[MAX_SQL] = "";
    while (line) {
        strip_line_comment(line);
        trim_spaces_inplace(line);
        if (!line[0]) {
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        if (stmt[0] == '\0' && (strcasecmp(line, "exit") == 0 || strcasecmp(line, "quit") == 0)) return rc;
        if (stmt[0] == '\0' && line[0] == '\\') {
            if (handle_slash_command(line)) return rc;
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        if (strlen(stmt) + strlen(line) + 2 >= sizeof(stmt)) {
            fprintf(stderr, "statement too long\n");
            return 2;
        }
        strcat(stmt, line);
        strcat(stmt, " ");
        char *semi;
        while ((semi = strchr(stmt, ';'))) {
            *semi = '\0';
            trim_inplace(stmt);
            if (stmt[0]) rc = execute_sql(stmt, dry_run, count);
            memmove(stmt, semi + 1, strlen(semi + 1) + 1);
            trim_spaces_inplace(stmt);
        }
        line = strtok_r(NULL, "\n", &save);
    }
    trim_inplace(stmt);
    if (stmt[0]) rc = execute_sql(stmt, dry_run, count);
    return rc;
}

int main(int argc, char **argv) {
    bool dry_run = false;
    bool count = false;
    const char *sql_arg = NULL;
    const char *file_arg = NULL;
    const char *fingerprint_type = NULL;
    const char *fingerprint_title = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "--fingerprint") == 0 && i + 2 < argc) {
            fingerprint_type = argv[++i];
            fingerprint_title = argv[++i];
        } else if (strcmp(argv[i], "--dry-run") == 0) dry_run = true;
        else if (strcmp(argv[i], "--count") == 0) count = true;
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) sql_arg = argv[++i];
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) file_arg = argv[++i];
        else sql_arg = argv[i];
    }
    if (fingerprint_type || fingerprint_title) return print_fingerprint(fingerprint_type, fingerprint_title);
    char *stdin_sql = NULL;
    char *file_sql = NULL;
    const char *sql = sql_arg;
    if (file_arg) {
        file_sql = read_file_text(file_arg);
        if (!file_sql) return 2;
        sql = file_sql;
    }
    if (!sql && isatty(STDIN_FILENO)) return repl(dry_run, count);
    if (!sql) {
        stdin_sql = read_stdin_sql();
        sql = stdin_sql;
    }
    if (!sql || !*sql) {
        usage();
        free(stdin_sql);
        free(file_sql);
        return 2;
    }
    int rc = (stdin_sql || file_sql || strchr(sql, '\n')) ? process_script((char *)sql, dry_run, count) : execute_sql(sql, dry_run, count);
    free(stdin_sql);
    free(file_sql);
    return rc;
}
