/*
 * Shared Windows answer-file generator for the Lufus GUI and CLI backends.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "windows_options.h"

#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

static GQuark windows_options_error(void)
{
    return g_quark_from_static_string("lufus-windows-options-error");
}

static gboolean architecture_valid(const char *architecture)
{
    return g_strcmp0(architecture, "amd64") == 0 ||
           g_strcmp0(architecture, "x86") == 0 ||
           g_strcmp0(architecture, "arm64") == 0 ||
           g_strcmp0(architecture, "arm") == 0;
}

static gboolean account_valid(const char *account)
{
    static const char *reserved[] = {
        "Administrator", "Guest", "DefaultAccount", "WDAGUtilityAccount",
        "HelpAssistant", "KRBTGT", "Local", "NONE", "SYSTEM", NULL
    };
    const char *invalid = "\"/\\[]:;|=,+*?<>";
    guint index;
    if (account == NULL || account[0] == '\0' || strlen(account) > 20 ||
        strpbrk(account, invalid) != NULL || account[strlen(account) - 1] == '.')
        return FALSE;
    for (index = 0; reserved[index] != NULL; index++) {
        if (g_ascii_strcasecmp(account, reserved[index]) == 0)
            return FALSE;
    }
    return TRUE;
}

static gboolean language_valid(const char *language)
{
    const char *cursor;
    gsize length;

    if (language == NULL)
        return TRUE;
    length = strlen(language);
    if (length < 2 || length > 35)
        return FALSE;
    for (cursor = language; *cursor != '\0'; cursor++) {
        if (!g_ascii_isalnum(*cursor) && *cursor != '-')
            return FALSE;
    }
    return TRUE;
}

static void component_open(GString *xml, const char *name, const char *architecture)
{
    g_string_append_printf(
        xml,
        "    <component name=\"%s\" processorArchitecture=\"%s\" language=\"neutral\" "
        "xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "publicKeyToken=\"31bf3856ad364e35\" versionScope=\"nonSxS\">\n",
        name, architecture);
}

void lufus_windows_options_clear(LufusWindowsOptions *options)
{
    if (options == NULL)
        return;
    g_clear_pointer(&options->architecture, g_free);
    g_clear_pointer(&options->language, g_free);
    g_clear_pointer(&options->local_account, g_free);
    options->flags = 0;
    options->image_index = 0;
    options->boot_media_partition = 0;
}

gboolean lufus_windows_options_validate(const LufusWindowsOptions *options,
                                        GError **error)
{
    const char *architecture;
    g_return_val_if_fail(options != NULL, FALSE);
    architecture = options->architecture != NULL ? options->architecture : "amd64";
    if ((options->flags & ~LUFUS_WINDOWS_ALL_FLAGS) != 0) {
        g_set_error(error, windows_options_error(), 3,
                    "Unknown Windows option flags: 0x%x",
                    options->flags & ~LUFUS_WINDOWS_ALL_FLAGS);
        return FALSE;
    }
    if (!architecture_valid(architecture)) {
        g_set_error(error, windows_options_error(), 1,
                    "Unsupported Windows architecture: %s", architecture);
        return FALSE;
    }
    if (!language_valid(options->language)) {
        g_set_error(error, windows_options_error(), 6,
                    "Invalid Windows language: %s", options->language);
        return FALSE;
    }
    if ((options->flags & LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT) != 0 &&
        !account_valid(options->local_account)) {
        g_set_error_literal(error, windows_options_error(), 2,
                            "Local account name is empty, reserved, or contains invalid characters");
        return FALSE;
    }
    if ((options->flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0 &&
        options->image_index == 0) {
        g_set_error_literal(error, windows_options_error(), 4,
                            "Silent Windows installation requires a 1-based image index");
        return FALSE;
    }
    if (options->boot_media_partition > 99) {
        g_set_error_literal(error, windows_options_error(), 5,
                            "Invalid installer-media partition index");
        return FALSE;
    }
    return TRUE;
}

static void append_silent_install(GString *xml,
                                  const LufusWindowsOptions *options)
{
    guint media_partition = options->boot_media_partition != 0 ?
                            options->boot_media_partition : 2;

    /* This mirrors Rufus' safety interlock.  Labelling the known partition on
     * Disk 1 must succeed before Setup is allowed to wipe Disk 0.  If the USB
     * is not Disk 1, or additional disks make the numbering ambiguous,
     * WillShowUI=OnError returns control to Windows Setup instead of guessing. */
    g_string_append_printf(
        xml,
        "      <DiskConfiguration>\n"
        "        <WillShowUI>OnError</WillShowUI>\n"
        "        <Disk wcm:action=\"modify\">\n"
        "          <DiskID>1</DiskID>\n"
        "          <ModifyPartitions>\n"
        "            <ModifyPartition wcm:action=\"modify\">\n"
        "              <Order>1</Order>\n"
        "              <PartitionID>%u</PartitionID>\n"
        "              <Label>LUFUS_BOOT</Label>\n"
        "            </ModifyPartition>\n"
        "          </ModifyPartitions>\n"
        "        </Disk>\n"
        "        <Disk wcm:action=\"add\">\n"
        "          <DiskID>0</DiskID>\n"
        "          <WillWipeDisk>true</WillWipeDisk>\n"
        "          <CreatePartitions>\n"
        "            <CreatePartition wcm:action=\"add\">\n"
        "              <Order>1</Order>\n"
        "              <Type>EFI</Type>\n"
        "              <Size>260</Size>\n"
        "            </CreatePartition>\n"
        "            <CreatePartition wcm:action=\"add\">\n"
        "              <Order>2</Order>\n"
        "              <Type>MSR</Type>\n"
        "              <Size>16</Size>\n"
        "            </CreatePartition>\n"
        "            <CreatePartition wcm:action=\"add\">\n"
        "              <Order>3</Order>\n"
        "              <Type>Primary</Type>\n"
        "              <Extend>true</Extend>\n"
        "            </CreatePartition>\n"
        "          </CreatePartitions>\n"
        "          <ModifyPartitions>\n"
        "            <ModifyPartition wcm:action=\"add\">\n"
        "              <Order>1</Order>\n"
        "              <PartitionID>1</PartitionID>\n"
        "              <Label>EFI</Label>\n"
        "              <Format>FAT32</Format>\n"
        "            </ModifyPartition>\n"
        "            <ModifyPartition wcm:action=\"add\">\n"
        "              <Order>2</Order>\n"
        "              <PartitionID>3</PartitionID>\n"
        "              <Label>Windows</Label>\n"
        "              <Letter>C</Letter>\n"
        "              <Format>NTFS</Format>\n"
        "            </ModifyPartition>\n"
        "          </ModifyPartitions>\n"
        "        </Disk>\n"
        "      </DiskConfiguration>\n"
        "      <ImageInstall>\n"
        "        <OSImage>\n"
        "          <WillShowUI>OnError</WillShowUI>\n"
        "          <InstallFrom>\n"
        "            <MetaData wcm:action=\"add\">\n"
        "              <Key>/IMAGE/INDEX</Key>\n"
        "              <Value>%u</Value>\n"
        "            </MetaData>\n"
        "          </InstallFrom>\n"
        "          <InstallTo>\n"
        "            <DiskID>0</DiskID>\n"
        "            <PartitionID>3</PartitionID>\n"
        "          </InstallTo>\n"
        "        </OSImage>\n"
        "      </ImageInstall>\n",
        media_partition, options->image_index);
}

gboolean lufus_windows_write_unattend(const char *path,
                                      const LufusWindowsOptions *options,
                                      GError **error)
{
    GString *xml;
    char *escaped_account = NULL;
    gboolean success;
    guint flags;
    const char *architecture;
    const char *language;

    g_return_val_if_fail(path != NULL, FALSE);
    g_return_val_if_fail(options != NULL, FALSE);
    flags = options->flags;
    architecture = options->architecture != NULL ? options->architecture : "amd64";
    language = options->language != NULL ? options->language : "en-US";
    if (!lufus_windows_options_validate(options, error))
        return FALSE;
    if ((flags & LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT) != 0) {
        escaped_account = g_markup_escape_text(options->local_account, -1);
    }

    xml = g_string_new("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                       "<unattend xmlns=\"urn:schemas-microsoft-com:unattend\">\n");
    if ((flags & (LUFUS_WINDOWS_BYPASS_REQUIREMENTS |
                  LUFUS_WINDOWS_SILENT_INSTALL)) != 0) {
        static const char *checks[] = {
            "BypassTPMCheck", "BypassSecureBootCheck", "BypassRAMCheck"
        };
        g_string_append(xml, "  <settings pass=\"windowsPE\">\n");
        component_open(xml, "Microsoft-Windows-Setup", architecture);
        g_string_append(xml,
                        "      <UserData><AcceptEula>true</AcceptEula>"
                        "<ProductKey><Key /></ProductKey></UserData>\n");
        if ((flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0)
            append_silent_install(xml, options);
        if ((flags & LUFUS_WINDOWS_BYPASS_REQUIREMENTS) != 0) {
            guint index;
            g_string_append(xml, "      <RunSynchronous>\n");
            for (index = 0; index < G_N_ELEMENTS(checks); index++) {
                g_string_append_printf(
                    xml,
                    "        <RunSynchronousCommand wcm:action=\"add\"><Order>%u</Order>"
                    "<Path>reg add HKLM\\SYSTEM\\Setup\\LabConfig /v %s "
                    "/t REG_DWORD /d 1 /f</Path></RunSynchronousCommand>\n",
                    index + 1, checks[index]);
            }
            g_string_append(xml, "      </RunSynchronous>\n");
        }
        g_string_append(xml, "    </component>\n");
        if ((flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
            component_open(xml, "Microsoft-Windows-International-Core-WinPE", architecture);
            g_string_append_printf(xml, "      <UILanguage>%s</UILanguage>\n", language);
            g_string_append(xml, "    </component>\n");
        }
        g_string_append(xml, "  </settings>\n");
    }
    if ((flags & LUFUS_WINDOWS_NO_ONLINE_ACCOUNT) != 0) {
        g_string_append(xml, "  <settings pass=\"specialize\">\n");
        component_open(xml, "Microsoft-Windows-Deployment", architecture);
        g_string_append(xml,
                        "      <RunSynchronous><RunSynchronousCommand wcm:action=\"add\">"
                        "<Order>1</Order><Path>reg add \"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\OOBE\" "
                        "/v BypassNRO /t REG_DWORD /d 1 /f</Path>"
                        "</RunSynchronousCommand></RunSynchronous>\n"
                        "    </component>\n  </settings>\n");
    }
    if ((flags & (LUFUS_WINDOWS_NO_DATA_COLLECTION |
                  LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT |
                  LUFUS_WINDOWS_DISABLE_BITLOCKER |
                  LUFUS_WINDOWS_SILENT_INSTALL)) != 0) {
        g_string_append(xml, "  <settings pass=\"oobeSystem\">\n");
        if ((flags & (LUFUS_WINDOWS_NO_DATA_COLLECTION |
                      LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT |
                      LUFUS_WINDOWS_SILENT_INSTALL)) != 0) {
            component_open(xml, "Microsoft-Windows-Shell-Setup", architecture);
            if ((flags & (LUFUS_WINDOWS_NO_DATA_COLLECTION |
                          LUFUS_WINDOWS_SILENT_INSTALL)) != 0) {
                g_string_append(xml,
                                "      <OOBE><HideEULAPage>true</HideEULAPage>"
                                "<ProtectYourPC>3</ProtectYourPC>");
                if ((flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0)
                    g_string_append(xml,
                                    "<HideOnlineAccountScreens>true</HideOnlineAccountScreens>"
                                    "<HideWirelessSetupInOOBE>true</HideWirelessSetupInOOBE>");
                g_string_append(xml, "</OOBE>\n");
            }
            if ((flags & LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT) != 0) {
                g_string_append_printf(
                    xml,
                    "      <UserAccounts><LocalAccounts><LocalAccount wcm:action=\"add\">"
                    "<Name>%s</Name><DisplayName>%s</DisplayName>"
                    "<Group>Administrators;Power Users</Group>"
                    "<Password><Value>UABhAHMAcwB3AG8AcgBkAA==</Value><PlainText>false</PlainText></Password>"
                    "</LocalAccount></LocalAccounts></UserAccounts>\n"
                    "      <FirstLogonCommands>"
                    "<SynchronousCommand wcm:action=\"add\"><Order>1</Order>"
                    "<CommandLine>net user &quot;%s&quot; /logonpasswordchg:yes</CommandLine>"
                    "</SynchronousCommand>"
                    "<SynchronousCommand wcm:action=\"add\"><Order>2</Order>"
                    "<CommandLine>net accounts /maxpwage:unlimited</CommandLine>"
                    "</SynchronousCommand></FirstLogonCommands>\n",
                    escaped_account, escaped_account, escaped_account);
            }
            g_string_append(xml, "    </component>\n");
        }
        if ((flags & LUFUS_WINDOWS_DISABLE_BITLOCKER) != 0) {
            component_open(xml, "Microsoft-Windows-SecureStartup-FilterDriver", architecture);
            g_string_append(xml, "      <PreventDeviceEncryption>true</PreventDeviceEncryption>\n    </component>\n");
            component_open(xml, "Microsoft-Windows-EnhancedStorage-Adm", architecture);
            g_string_append(xml, "      <TCGSecurityActivationDisabled>1</TCGSecurityActivationDisabled>\n    </component>\n");
        }
        if ((flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
            component_open(xml, "Microsoft-Windows-International-Core", architecture);
            g_string_append_printf(
                xml,
                "      <InputLocale>%s</InputLocale>\n"
                "      <SystemLocale>%s</SystemLocale>\n"
                "      <UserLocale>%s</UserLocale>\n"
                "      <UILanguage>%s</UILanguage>\n",
                language, language, language, language);
            g_string_append(xml, "    </component>\n");
        }
        g_string_append(xml, "  </settings>\n");
    }
    if ((flags & LUFUS_WINDOWS_OFFLINE_INTERNAL_DRIVES) != 0) {
        g_string_append(xml, "  <settings pass=\"offlineServicing\">\n");
        component_open(xml, "Microsoft-Windows-PartitionManager", architecture);
        g_string_append(xml, "      <SanPolicy>4</SanPolicy>\n    </component>\n  </settings>\n");
    }
    g_string_append(xml, "</unattend>\n");
    success = g_file_set_contents(path, xml->str, (gssize)xml->len, error);
    g_string_free(xml, TRUE);
    g_free(escaped_account);
    return success;
}
