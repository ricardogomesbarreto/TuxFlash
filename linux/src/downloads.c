/*
 * Lufus official-image discovery and download support
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This implements the public Microsoft retail-ISO workflow used by the
 * GPL-3.0-or-later Fido project. It does not proxy, mirror, authenticate to,
 * or modify Microsoft images: the returned temporary URL is hosted by
 * Microsoft and the ISO is downloaded byte-for-byte from that URL.
 */

#include "downloads.h"

#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

#define MICROSOFT_PROFILE "606624d44113"
#define MICROSOFT_ORG "y6jn8c31"
#define MICROSOFT_INSTANCE "560dc9f3-1aa5-4a2f-b63c-9e18f8d0e175"
#define LUFUS_USER_AGENT "TuxFlash/0.3"

static SoupSession *new_http_session(guint timeout)
{
    SoupSession *session = soup_session_new_with_options(
        "user-agent", LUFUS_USER_AGENT, "timeout", timeout, NULL);
    g_autofree char *executable = g_file_read_link("/proc/self/exe", NULL);
    if (executable != NULL) {
        g_autofree char *directory = g_path_get_dirname(executable);
        g_autofree char *prefix = g_path_get_dirname(directory);
        g_autofree char *anchors = g_build_filename(prefix, "share", "tuxflash",
                                                   "certs", "ca-certificates.crt", NULL);
        if (g_file_test(anchors, G_FILE_TEST_IS_REGULAR)) {
            GError *error = NULL;
            GTlsDatabase *database = g_tls_file_database_new(anchors, &error);
            /* NULL explicitly fails certificate verification; it never disables TLS. */
            soup_session_set_tls_database(session, database);
            g_clear_object(&database);
            if (error != NULL) {
                g_warning("Could not load the bundled certificate authorities: %s", error->message);
                g_error_free(error);
            }
        }
    }
    return session;
}

typedef struct {
    const char *id;
    const char *architecture;
} EditionDefinition;

typedef struct {
    const char *key;
    const char *name;
    const char *download_page;
    const EditionDefinition editions[3];
} ReleaseDefinition;

static const ReleaseDefinition releases[] = {
    {
        "windows11", "Windows 11 25H2 v2 (Home/Pro/Education)",
        "https://www.microsoft.com/software-download/windows11",
        {{"3321", "x64"}, {"3324", "arm64"}, {NULL, NULL}}
    },
    {
        "windows10", "Windows 10 22H2 (Home/Pro/Education)",
        "https://www.microsoft.com/software-download/windows10ISO",
        {{"2618", NULL}, {NULL, NULL}}
    },
};

static const ReleaseDefinition *find_release(const char *key)
{
    guint index;
    for (index = 0; index < G_N_ELEMENTS(releases); index++) {
        if (g_strcmp0(key, releases[index].key) == 0)
            return &releases[index];
    }
    return NULL;
}

const char *lufus_windows_release_name(const char *release)
{
    const ReleaseDefinition *definition = find_release(release);
    return definition != NULL ? definition->name : NULL;
}

const char *lufus_windows_release_download_page(const char *release)
{
    const ReleaseDefinition *definition = find_release(release);
    return definition != NULL ? definition->download_page : NULL;
}

static void sku_free(LufusWindowsSku *sku)
{
    g_free(sku->session_id);
    g_free(sku->sku_id);
    g_free(sku->architecture);
    g_free(sku);
}

static void language_free(LufusWindowsLanguage *language)
{
    g_free(language->name);
    g_free(language->display_name);
    g_ptr_array_unref(language->skus);
    g_free(language);
}

static GBytes *http_get(SoupSession *session, const char *url,
                        const char *referer, GCancellable *cancellable,
                        GError **error)
{
    SoupMessage *message = soup_message_new("GET", url);
    GBytes *bytes;
    guint status;
    if (message == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Invalid download-service URL");
        return NULL;
    }
    if (referer != NULL)
        soup_message_headers_append(soup_message_get_request_headers(message),
                                    "Referer", referer);
    bytes = soup_session_send_and_read(session, message, cancellable, error);
    status = soup_message_get_status(message);
    if (bytes != NULL && !SOUP_STATUS_IS_SUCCESSFUL(status)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "Download service returned HTTP %u (%s)", status,
                    soup_message_get_reason_phrase(message));
        g_clear_pointer(&bytes, g_bytes_unref);
    }
    g_object_unref(message);
    return bytes;
}

static char *bytes_to_string(GBytes *bytes)
{
    gsize length;
    const void *data = g_bytes_get_data(bytes, &length);
    return g_strndup(data, length);
}

static char *extract_regex(const char *text, const char *pattern)
{
    GRegex *regex = g_regex_new(pattern, 0, 0, NULL);
    GMatchInfo *match = NULL;
    char *value = NULL;
    if (g_regex_match(regex, text, 0, &match))
        value = g_match_info_fetch(match, 1);
    g_match_info_free(match);
    g_regex_unref(regex);
    return value;
}

static gboolean bootstrap_session(SoupSession *http, const char *session_id,
                                  GCancellable *cancellable, GError **error)
{
    GBytes *bytes = NULL;
    char *url = NULL;
    char *script = NULL;
    char *w = NULL;
    char *rticks = NULL;
    gboolean success = FALSE;

    url = g_strdup_printf("https://vlscppe.microsoft.com/tags?org_id=%s&session_id=%s",
                          MICROSOFT_ORG, session_id);
    bytes = http_get(http, url, NULL, cancellable, error);
    g_clear_pointer(&bytes, g_bytes_unref);
    g_clear_pointer(&url, g_free);
    if (error != NULL && *error != NULL)
        goto out;

    url = g_strdup_printf("https://ov-df.microsoft.com/mdt.js?instanceId=%s&PageId=si&session_id=%s",
                          MICROSOFT_INSTANCE, session_id);
    bytes = http_get(http, url, NULL, cancellable, error);
    g_clear_pointer(&url, g_free);
    if (bytes == NULL)
        goto out;
    script = bytes_to_string(bytes);
    g_clear_pointer(&bytes, g_bytes_unref);
    w = extract_regex(script, "[?&]w=([A-F0-9]+)");
    rticks = extract_regex(script, "rticks[^0-9]{0,12}([0-9]+)");
    if (w == NULL || rticks == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Microsoft session bootstrap response changed");
        goto out;
    }
    url = g_strdup_printf("https://ov-df.microsoft.com/?session_id=%s&CustomerId=%s&PageId=si&w=%s&mdt=%" G_GINT64_FORMAT "&rticks=%s",
                          session_id, MICROSOFT_INSTANCE, w,
                          g_get_real_time() / 1000, rticks);
    bytes = http_get(http, url, NULL, cancellable, error);
    success = bytes != NULL;
out:
    g_clear_pointer(&bytes, g_bytes_unref);
    g_free(url);
    g_free(script);
    g_free(w);
    g_free(rticks);
    return success;
}

static LufusWindowsLanguage *find_language_mutable(GPtrArray *languages,
                                                    const char *name)
{
    guint index;
    for (index = 0; index < languages->len; index++) {
        LufusWindowsLanguage *language = g_ptr_array_index(languages, index);
        if (g_ascii_strcasecmp(language->name, name) == 0 ||
            g_ascii_strcasecmp(language->display_name, name) == 0)
            return language;
    }
    return NULL;
}

typedef struct {
    gint64 type;
    char *key;
    char *value;
} MicrosoftServiceError;

static void microsoft_service_error_clear(MicrosoftServiceError *service_error)
{
    g_free(service_error->key);
    g_free(service_error->value);
    memset(service_error, 0, sizeof(*service_error));
}

static gboolean microsoft_error(JsonObject *root,
                                MicrosoftServiceError *service_error)
{
    JsonArray *errors;
    JsonObject *item;
    memset(service_error, 0, sizeof(*service_error));
    if (root == NULL || !json_object_has_member(root, "Errors") ||
        json_object_get_null_member(root, "Errors"))
        return FALSE;
    errors = json_object_get_array_member(root, "Errors");
    if (errors == NULL || json_array_get_length(errors) == 0)
        return FALSE;
    item = json_array_get_object_element(errors, 0);
    if (item != NULL) {
        if (json_object_has_member(item, "Type"))
            service_error->type = json_object_get_int_member(item, "Type");
        if (json_object_has_member(item, "Key") &&
            !json_object_get_null_member(item, "Key"))
            service_error->key = g_strdup(json_object_get_string_member(item, "Key"));
        if (json_object_has_member(item, "Value") &&
            !json_object_get_null_member(item, "Value"))
            service_error->value = g_strdup(json_object_get_string_member(item, "Value"));
    }
    if (service_error->value == NULL)
        service_error->value = g_strdup("Microsoft rejected the download request");
    return TRUE;
}

gboolean lufus_windows_service_error_is_sentinel(gint64 type,
                                                  const char *key,
                                                  const char *value)
{
    return type == 9 ||
           g_strcmp0(key, "ErrorSettings.SentinelReject") == 0 ||
           (value != NULL && g_strrstr(value, "Sentinel") != NULL);
}

static void set_microsoft_service_error(GError **error,
                                        const MicrosoftServiceError *service_error,
                                        const char *session_id,
                                        const char *release)
{
    if (lufus_windows_service_error_is_sentinel(service_error->type,
                                                 service_error->key,
                                                 service_error->value)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                    "Microsoft rejected automatic ISO link generation "
                    "(message code 715-123130; session %s). Microsoft may "
                    "restrict an IP, network, entity, or location, including "
                    "anonymous or location-hiding services. Wait and retry "
                    "later, or use Microsoft's official download page: %s",
                    session_id,
                    lufus_windows_release_download_page(release));
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "Microsoft rejected the automatic ISO request: %s",
                    service_error->value);
    }
}

static gboolean request_edition_languages(LufusWindowsCatalog *catalog,
                                          const EditionDefinition *edition,
                                          const char *session_id,
                                          GCancellable *cancellable,
                                          GError **error)
{
    SoupSession *http = catalog->session;
    char *locale = g_uri_escape_string(catalog->locale, NULL, TRUE);
    char *url = NULL;
    GBytes *bytes = NULL;
    JsonParser *parser = NULL;
    JsonObject *root;
    JsonArray *skus;
    guint index;
    guint added_skus = 0;
    gboolean success = FALSE;

    url = g_strdup_printf("https://www.microsoft.com/software-download-connector/api/getskuinformationbyproductedition?profile=%s&productEditionId=%s&SKU=undefined&friendlyFileName=undefined&Locale=%s&sessionID=%s",
                          MICROSOFT_PROFILE, edition->id, locale, session_id);
    bytes = http_get(http, url, NULL, cancellable, error);
    if (bytes == NULL)
        goto out;
    parser = json_parser_new();
    {
        gsize length;
        const char *data = g_bytes_get_data(bytes, &length);
        if (!json_parser_load_from_data(parser, data, (gssize)length, error))
            goto out;
    }
    root = json_node_get_object(json_parser_get_root(parser));
    if (root == NULL || !json_object_has_member(root, "Skus")) {
        MicrosoftServiceError service_error;
        if (microsoft_error(root, &service_error)) {
            set_microsoft_service_error(error, &service_error, session_id,
                                        catalog->release);
            microsoft_service_error_clear(&service_error);
        } else {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                "Microsoft returned no Windows language catalog");
        }
        goto out;
    }
    skus = json_object_get_array_member(root, "Skus");
    for (index = 0; index < json_array_get_length(skus); index++) {
        JsonObject *item = json_array_get_object_element(skus, index);
        const char *name = json_object_get_string_member(item, "Language");
        const char *display = json_object_get_string_member(item, "LocalizedLanguage");
        const char *sku_id = json_object_get_string_member(item, "Id");
        LufusWindowsLanguage *language = find_language_mutable(catalog->languages, name);
        LufusWindowsSku *sku;
        if (language == NULL) {
            language = g_new0(LufusWindowsLanguage, 1);
            language->name = g_strdup(name);
            language->display_name = g_strdup(display);
            language->skus = g_ptr_array_new_with_free_func((GDestroyNotify)sku_free);
            g_ptr_array_add(catalog->languages, language);
        }
        sku = g_new0(LufusWindowsSku, 1);
        sku->session_id = g_strdup(session_id);
        sku->sku_id = g_strdup(sku_id);
        sku->architecture = g_strdup(edition->architecture);
        g_ptr_array_add(language->skus, sku);
        added_skus++;
    }
    success = added_skus > 0;
    if (!success)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Microsoft returned an empty Windows language catalog");
out:
    g_clear_object(&parser);
    g_clear_pointer(&bytes, g_bytes_unref);
    g_free(url);
    g_free(locale);
    return success;
}

static gboolean add_edition_languages(LufusWindowsCatalog *catalog,
                                      const EditionDefinition *edition,
                                      GCancellable *cancellable, GError **error)
{
    char *session_id = g_uuid_string_random();
    guint attempt;
    gboolean added = FALSE;
    if (!bootstrap_session(catalog->session, session_id, cancellable, error))
        goto out;
    for (attempt = 0; attempt < 3 && !added; attempt++) {
        GError *attempt_error = NULL;
        added = request_edition_languages(catalog, edition, session_id,
                                          cancellable, &attempt_error);
        if (added)
            break;
        if (attempt == 2 ||
            g_error_matches(attempt_error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
            g_error_matches(attempt_error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)) {
            g_propagate_error(error, attempt_error);
            break;
        }
        g_clear_error(&attempt_error);
        g_usleep(2U * G_USEC_PER_SEC);
    }
out:
    g_free(session_id);
    return added;
}

static gboolean edition_supports_architecture(const EditionDefinition *edition,
                                              const char *architecture)
{
    return architecture == NULL || architecture[0] == '\0' ||
           edition->architecture == NULL ||
           g_ascii_strcasecmp(edition->architecture, architecture) == 0;
}

LufusWindowsCatalog *lufus_windows_catalog_discover_for_architecture(
    const char *release, const char *locale, const char *architecture,
    GCancellable *cancellable, GError **error)
{
    const ReleaseDefinition *definition = find_release(release);
    LufusWindowsCatalog *catalog;
    guint index;
    gboolean found_edition = FALSE;
    if (definition == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                    "Unsupported Windows release '%s'", release);
        return NULL;
    }
    catalog = g_new0(LufusWindowsCatalog, 1);
    catalog->release = g_strdup(definition->key);
    catalog->release_name = g_strdup(definition->name);
    catalog->locale = g_strdup(locale != NULL && locale[0] != '\0' ? locale : "en-US");
    catalog->languages = g_ptr_array_new_with_free_func((GDestroyNotify)language_free);
    catalog->session = new_http_session(45);
    for (index = 0; definition->editions[index].id != NULL; index++) {
        if (!edition_supports_architecture(&definition->editions[index], architecture))
            continue;
        found_edition = TRUE;
        if (!add_edition_languages(catalog, &definition->editions[index],
                                   cancellable, error)) {
            lufus_windows_catalog_free(catalog);
            return NULL;
        }
    }
    if (!found_edition) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                    "%s is not offered for %s", architecture,
                    definition->name);
        lufus_windows_catalog_free(catalog);
        return NULL;
    }
    return catalog;
}

LufusWindowsCatalog *lufus_windows_catalog_discover(const char *release,
                                                     const char *locale,
                                                     GCancellable *cancellable,
                                                     GError **error)
{
    return lufus_windows_catalog_discover_for_architecture(
        release, locale, NULL, cancellable, error);
}

void lufus_windows_catalog_free(LufusWindowsCatalog *catalog)
{
    if (catalog == NULL)
        return;
    g_free(catalog->release);
    g_free(catalog->release_name);
    g_free(catalog->locale);
    g_ptr_array_unref(catalog->languages);
    g_clear_object((GObject **)&catalog->session);
    g_free(catalog);
}

const LufusWindowsLanguage *lufus_windows_catalog_find_language(
    const LufusWindowsCatalog *catalog, const char *name)
{
    return catalog != NULL && name != NULL ?
           find_language_mutable(catalog->languages, name) : NULL;
}

static const char *architecture_for_type(gint64 type)
{
    switch (type) {
    case 0: return "x86";
    case 1: return "x64";
    case 2: return "arm64";
    default: return "unknown";
    }
}

char *lufus_windows_catalog_resolve(const LufusWindowsCatalog *catalog,
                                    const LufusWindowsLanguage *language,
                                    const char *architecture,
                                    GCancellable *cancellable,
                                    GError **error)
{
    guint source_index;
    char *escaped_locale = g_uri_escape_string(catalog->locale, NULL, TRUE);
    char *result = NULL;
    for (source_index = 0; source_index < language->skus->len && result == NULL;
         source_index++) {
        LufusWindowsSku *sku = g_ptr_array_index(language->skus, source_index);
        char *url;
        GBytes *bytes;
        JsonParser *parser = NULL;
        gboolean service_rejected = FALSE;
        if (sku->architecture != NULL &&
            g_ascii_strcasecmp(sku->architecture, architecture) != 0)
            continue;
        url = g_strdup_printf("https://www.microsoft.com/software-download-connector/api/GetProductDownloadLinksBySku?profile=%s&productEditionId=undefined&SKU=%s&friendlyFileName=undefined&Locale=%s&sessionID=%s",
                              MICROSOFT_PROFILE, sku->sku_id, escaped_locale,
                              sku->session_id);
        bytes = http_get(catalog->session, url,
            "https://www.microsoft.com/software-download/windows11",
            cancellable, error);
        g_free(url);
        if (bytes == NULL)
            break;
        parser = json_parser_new();
        {
            gsize length;
            const char *data = g_bytes_get_data(bytes, &length);
            if (json_parser_load_from_data(parser, data, (gssize)length, error)) {
                JsonObject *root = json_node_get_object(json_parser_get_root(parser));
                MicrosoftServiceError service_error;
                if (microsoft_error(root, &service_error)) {
                    set_microsoft_service_error(error, &service_error,
                                                sku->session_id,
                                                catalog->release);
                    microsoft_service_error_clear(&service_error);
                    service_rejected = TRUE;
                } else if (root != NULL &&
                           json_object_has_member(root, "ProductDownloadOptions")) {
                    JsonArray *options = json_object_get_null_member(root, "ProductDownloadOptions") ?
                                         NULL : json_object_get_array_member(root, "ProductDownloadOptions");
                    guint option_index;
                    for (option_index = 0; options != NULL && option_index < json_array_get_length(options);
                         option_index++) {
                        JsonObject *option = json_array_get_object_element(options, option_index);
                        const char *found_architecture = architecture_for_type(
                            json_object_get_int_member(option, "DownloadType"));
                        if (g_ascii_strcasecmp(found_architecture, architecture) == 0 &&
                            json_object_has_member(option, "Uri") &&
                            !json_object_get_null_member(option, "Uri")) {
                            result = g_strdup(json_object_get_string_member(option, "Uri"));
                            break;
                        }
                    }
                }
            }
        }
        g_object_unref(parser);
        g_bytes_unref(bytes);
        if (service_rejected)
            break;
        if (error != NULL && *error != NULL)
            break;
    }
    g_free(escaped_locale);
    if (result == NULL && (error == NULL || *error == NULL)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    "%s is not offered for %s", architecture, language->display_name);
    }
    return result;
}

gboolean lufus_download_url(const char *url, const char *output,
                            LufusDownloadProgress progress, gpointer user_data,
                            GCancellable *cancellable, GError **error)
{
    SoupSession *session = new_http_session(60);
    SoupMessage *message = soup_message_new("GET", url);
    GInputStream *input = NULL;
    GFile *file = g_file_new_for_path(output);
    GFileOutputStream *output_stream = NULL;
    guint64 completed = 0;
    guint64 total = 0;
    unsigned char buffer[1024 * 1024];
    gssize count;
    gboolean success = FALSE;
    gboolean created = FALSE;
    if (message == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Invalid download URL");
        goto out;
    }
    input = soup_session_send(session, message, cancellable, error);
    if (input == NULL)
        goto out;
    if (!SOUP_STATUS_IS_SUCCESSFUL(soup_message_get_status(message))) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "Download returned HTTP %u", soup_message_get_status(message));
        goto out;
    }
    total = soup_message_headers_get_content_length(soup_message_get_response_headers(message));
    output_stream = g_file_create(file, G_FILE_CREATE_PRIVATE, cancellable, error);
    if (output_stream == NULL)
        goto out;
    created = TRUE;
    while ((count = g_input_stream_read(input, buffer, sizeof(buffer), cancellable, error)) > 0) {
        gsize written = 0;
        if (!g_output_stream_write_all(G_OUTPUT_STREAM(output_stream), buffer, (gsize)count,
                                       &written, cancellable, error))
            goto out;
        completed += written;
        if (progress != NULL)
            progress(completed, total, user_data);
    }
    if (count < 0 || !g_output_stream_close(G_OUTPUT_STREAM(output_stream), cancellable, error))
        goto out;
    success = TRUE;
out:
    if (!success && created)
        g_file_delete(file, NULL, NULL);
    g_clear_object(&output_stream);
    g_clear_object(&input);
    g_clear_object(&file);
    g_clear_object(&message);
    g_clear_object(&session);
    return success;
}
