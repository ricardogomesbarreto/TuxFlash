/*
 * Minimal native .reg importer for BCD-SYS's fixed registry templates.
 *
 * It intentionally supports only the syntax used by the pinned BCD-SYS
 * winload.reg and recovery.reg files: keys, REG_SZ, REG_DWORD, REG_BINARY and
 * REG_MULTI_SZ. This replaces the removed Debian hivexregedit Perl frontend
 * without adding a private language runtime to the Lufus package.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <errno.h>
#include <hivex.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>

static gboolean copy_file(const char *source, const char *destination,
                          GError **error)
{
    char *contents = NULL;
    gsize length = 0;
    gboolean result;
    if (!g_file_get_contents(source, &contents, &length, error))
        return FALSE;
    result = g_file_set_contents(destination, contents, (gssize)length, error);
    g_free(contents);
    return result;
}
static gboolean parse_hex_bytes(const char *text, GByteArray **result,
                                GError **error)
{
    char **items = g_strsplit(text, ",", -1);
    GByteArray *bytes = g_byte_array_new();
    guint index;
    for (index = 0; items[index] != NULL; index++) {
        char *item = g_strstrip(items[index]);
        char *end = NULL;
        guint64 value;
        if (*item == '\0')
            continue;
        errno = 0;
        value = g_ascii_strtoull(item, &end, 16);
        if (errno != 0 || end == item || *end != '\0' || value > 0xff) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        "Invalid registry hex byte: %s", item);
            g_byte_array_unref(bytes);
            g_strfreev(items);
            return FALSE;
        }
        {
            guint8 byte = (guint8)value;
            g_byte_array_append(bytes, &byte, 1);
        }
    }
    g_strfreev(items);
    *result = bytes;
    return TRUE;
}

static gboolean parse_string_value(const char *text, GByteArray **result,
                                   GError **error)
{
    gsize length = strlen(text);
    GString *unescaped;
    char *utf16;
    gsize bytes_written = 0;
    gsize index;
    GByteArray *bytes;
    const guint8 terminator[2] = {0, 0};
    if (length < 2 || text[0] != '"' || text[length - 1] != '"') {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Invalid registry string: %s", text);
        return FALSE;
    }
    unescaped = g_string_new(NULL);
    for (index = 1; index + 1 < length; index++) {
        if (text[index] == '\\' && index + 2 < length)
            index++;
        g_string_append_c(unescaped, text[index]);
    }
    utf16 = g_convert(unescaped->str, (gssize)unescaped->len,
                      "UTF-16LE", "UTF-8", NULL, &bytes_written, error);
    g_string_free(unescaped, TRUE);
    if (utf16 == NULL)
        return FALSE;
    bytes = g_byte_array_sized_new(bytes_written + sizeof(terminator));
    g_byte_array_append(bytes, (const guint8 *)utf16, bytes_written);
    g_byte_array_append(bytes, terminator, sizeof(terminator));
    g_free(utf16);
    *result = bytes;
    return TRUE;
}

static hive_node_h ensure_path(hive_h *hive, const char *path, GError **error)
{
    char **parts = g_strsplit(path, "\\", -1);
    hive_node_h node = hivex_root(hive);
    guint index;
    if (node == 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Could not read registry root: %s", g_strerror(errno));
        g_strfreev(parts);
        return 0;
    }
    if (g_strcmp0(parts[0], "BCD00000001") != 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Unexpected registry prefix in [%s]", path);
        g_strfreev(parts);
        return 0;
    }
    for (index = 1; parts[index] != NULL; index++) {
        hive_node_h child;
        if (*parts[index] == '\0')
            continue;
        errno = 0;
        child = hivex_node_get_child(hive, node, parts[index]);
        if (child == 0 && (errno == 0 || errno == HIVEX_NO_KEY))
            child = hivex_node_add_child(hive, node, parts[index]);
        if (child == 0) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                        "Could not create registry key %s: %s",
                        parts[index], g_strerror(errno));
            g_strfreev(parts);
            return 0;
        }
        node = child;
    }
    g_strfreev(parts);
    return node;
}

static gboolean set_value(hive_h *hive, hive_node_h node, const char *line,
                          GError **error)
{
    const char *separator;
    const char *encoded;
    char *key;
    hive_type type;
    GByteArray *bytes = NULL;
    hive_set_value value;
    gboolean result = FALSE;

    if (line[0] != '"' || (separator = strstr(line + 1, "\"=")) == NULL) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Unsupported registry value line: %s", line);
        return FALSE;
    }
    key = g_strndup(line + 1, (gsize)(separator - line - 1));
    encoded = separator + 2;
    if (g_str_has_prefix(encoded, "dword:")) {
        char *end = NULL;
        guint64 number;
        guint8 data[4];
        errno = 0;
        number = g_ascii_strtoull(encoded + 6, &end, 16);
        if (errno != 0 || end == encoded + 6 || *end != '\0' ||
            number > UINT32_MAX) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        "Invalid registry DWORD: %s", encoded);
            goto out;
        }
        data[0] = (guint8)number;
        data[1] = (guint8)(number >> 8);
        data[2] = (guint8)(number >> 16);
        data[3] = (guint8)(number >> 24);
        bytes = g_byte_array_sized_new(sizeof(data));
        g_byte_array_append(bytes, data, sizeof(data));
        type = hive_t_REG_DWORD;
    } else if (g_str_has_prefix(encoded, "hex(7):")) {
        if (!parse_hex_bytes(encoded + 7, &bytes, error))
            goto out;
        type = hive_t_REG_MULTI_SZ;
    } else if (g_str_has_prefix(encoded, "hex:")) {
        if (!parse_hex_bytes(encoded + 4, &bytes, error))
            goto out;
        type = hive_t_REG_BINARY;
    } else if (encoded[0] == '"') {
        if (!parse_string_value(encoded, &bytes, error))
            goto out;
        type = hive_t_REG_SZ;
    } else {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Unsupported registry value encoding: %s", encoded);
        goto out;
    }
    value.key = key;
    value.t = type;
    value.len = bytes->len;
    value.value = (char *)bytes->data;
    if (hivex_node_set_value(hive, node, &value, 0) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Could not write registry value %s: %s", key,
                    g_strerror(errno));
        goto out;
    }
    result = TRUE;
out:
    if (bytes != NULL)
        g_byte_array_unref(bytes);
    g_free(key);
    return result;
}

static gboolean import_reg_file(hive_h *hive, const char *path, GError **error)
{
    char *contents = NULL;
    char **physical_lines = NULL;
    GString *logical = g_string_new(NULL);
    hive_node_h node = 0;
    guint index;
    gboolean result = FALSE;

    if (!g_file_get_contents(path, &contents, NULL, error))
        goto out;
    physical_lines = g_strsplit(contents, "\n", -1);
    for (index = 0; physical_lines[index] != NULL; index++) {
        char *line = g_strstrip(physical_lines[index]);
        gsize length = strlen(line);
        gboolean continued = length > 0 && line[length - 1] == '\\';
        if (continued)
            line[length - 1] = '\0';
        g_string_append(logical, line);
        if (continued)
            continue;
        line = g_strstrip(logical->str);
        if (*line == '\0' || *line == ';' ||
            g_strcmp0(line, "Windows Registry Editor Version 5.00") == 0) {
            g_string_truncate(logical, 0);
            continue;
        }
        length = strlen(line);
        if (line[0] == '[' && length >= 2 && line[length - 1] == ']') {
            line[length - 1] = '\0';
            node = ensure_path(hive, line + 1, error);
            if (node == 0)
                goto out;
        } else {
            if (node == 0) {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                            "Registry value appears before a key: %s", line);
                goto out;
            }
            if (!set_value(hive, node, line, error))
                goto out;
        }
        g_string_truncate(logical, 0);
    }
    if (logical->len != 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Unterminated continuation in %s", path);
        goto out;
    }
    result = TRUE;
out:
    g_string_free(logical, TRUE);
    g_strfreev(physical_lines);
    g_free(contents);
    return result;
}

int main(int argc, char **argv)
{
    GError *error = NULL;
    hive_h *hive;
    int status = 1;
    if (argc != 4) {
        g_printerr("Usage: %s BASE-HIVE REG-FILE OUTPUT-HIVE\n", argv[0]);
        return 2;
    }
    if (!copy_file(argv[1], argv[3], &error))
        goto out;
    hive = hivex_open(argv[3], HIVEX_OPEN_WRITE);
    if (hive == NULL) {
        g_set_error(&error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Could not open output hive: %s", g_strerror(errno));
        goto out;
    }
    if (!import_reg_file(hive, argv[2], &error)) {
        hivex_close(hive);
        goto out;
    }
    if (hivex_commit(hive, NULL, 0) != 0) {
        g_set_error(&error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Could not commit output hive: %s", g_strerror(errno));
        hivex_close(hive);
        goto out;
    }
    if (hivex_close(hive) != 0) {
        g_set_error(&error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Could not close output hive: %s", g_strerror(errno));
        goto out;
    }
    status = 0;
out:
    if (error != NULL) {
        g_printerr("%s\n", error->message);
        g_clear_error(&error);
    }
    return status;
}
