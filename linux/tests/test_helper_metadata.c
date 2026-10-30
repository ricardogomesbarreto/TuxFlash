/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main tuxflash_helper_entry
#include "../src/helper.c"
#undef main
#include <sys/prctl.h>

static int emit(int fd, const char *bytes, size_t length)
{
    while (length) {
        ssize_t n = write(fd, bytes, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        bytes += n; length -= (size_t)n;
    }
    return 0;
}
static int metadata_fixture(void)
{
    const char *mode = g_getenv("TUXFLASH_META_MODE");
    const char *dir = g_getenv("TUXFLASH_META_DIR");
    g_autofree char *ready = g_build_filename(dir, "ready", NULL);
    g_autofree char *pids = g_build_filename(dir, "pids", NULL);
    pid_t leaf = 0;
    if (strcmp(mode, "failed-pipe") == 0) {
        int started[2]; if (pipe(started) != 0) return 125;
        leaf = fork(); if (leaf < 0) return 125;
        if (!leaf) {
            close(started[0]); signal(SIGTERM, SIG_IGN);
            if (emit(started[1], "R", 1) != 0) _exit(125);
            close(started[1]); for (;;) pause();
        }
        close(started[1]); char byte;
        if (read(started[0], &byte, 1) != 1) return 125;
        close(started[0]);
    }
    if (g_str_has_prefix(mode, "cancel-")) signal(SIGTERM, SIG_IGN);
    g_autofree char *record = g_strdup_printf("%ld %ld\n", (long)getpid(), (long)leaf);
    if (!g_file_set_contents(pids, record, -1, NULL) ||
        !g_file_set_contents(ready, "ready", -1, NULL)) return 125;
    if (g_str_has_prefix(mode, "cancel-")) { for (;;) pause(); }
    if (strcmp(mode, "failed-pipe") == 0) { emit(2, "metadata fixture failed\n", 24); return 7; }
    if (strcmp(mode, "invalid") == 0) {
        const char bytes[] = "Default Language: pt-BR\n\xff";
        return emit(1, bytes, sizeof(bytes)-1) == 0 ? 0 : 1;
    }
    if (strcmp(mode, "nul") == 0) {
        const char bytes[] = "Default Language: pt-BR\n\0hidden";
        return emit(1, bytes, sizeof(bytes)-1) == 0 ? 0 : 1;
    }
    if (strcmp(mode, "limit-vhd") == 0) emit(1, "{\"virtual-size\":67108864}\n", 26);
    if (g_str_has_prefix(mode, "limit-")) {
        char block[65536]; memset(block, strcmp(mode,"limit-vhd")==0 ? ' ' : 'A', sizeof(block));
        for (int i=0;i<80;i++) if (emit(1, block, sizeof(block)) != 0) return 1;
    }
    if (strcmp(mode, "streams") == 0) {
        char block[8192]; memset(block,'W',sizeof(block));
        for (int i=0;i<20;i++) if (emit(2,block,sizeof(block)) != 0) return 1;
    }
    if (strcmp(mode, "limit-vhd") != 0) {
        const char text[]="Name: Fixture\nDefault Language: pt-BR\n";
        if (emit(1,text,sizeof(text)-1) != 0) return 1;
    }
    return 0;
}
static void reap_owned(pid_t pid)
{
    if (pid <= 1) return;
    siginfo_t info={0};
    if (waitid(P_PID,pid,&info,WEXITED|WNOHANG|WNOWAIT) != 0) return;
    if (!info.si_pid) kill(pid,SIGKILL);
    wait_child(pid,NULL);
}
static void metadata_case(const char *mode)
{
    g_assert_cmpint(prctl(PR_SET_CHILD_SUBREAPER,1),==,0);
    g_autofree char *dir=g_dir_make_tmp("tuxflash-metadata-XXXXXX",NULL);
    g_autofree char *ready=g_build_filename(dir,"ready",NULL);
    g_autofree char *pids=g_build_filename(dir,"pids",NULL);
    g_autofree char *wim=g_build_filename(dir,"wimlib-imagex",NULL);
    g_autofree char *qemu=g_build_filename(dir,"qemu-img",NULL);
    char executable[PATH_MAX]; ssize_t n=readlink("/proc/self/exe",executable,sizeof(executable)-1);
    g_assert_cmpint(n,>,0); executable[n]=0;
    g_autofree char *quoted=g_shell_quote(executable);
    g_autofree char *script=g_strdup_printf("#!/bin/sh\nexec %s --metadata-fixture\n",quoted);
    g_assert_true(g_file_set_contents(wim,script,-1,NULL)); g_assert_cmpint(chmod(wim,0700),==,0);
    g_assert_true(g_file_set_contents(qemu,script,-1,NULL)); g_assert_cmpint(chmod(qemu,0700),==,0);
    int control[2]={-1,-1};
    gboolean eof_cancel=strcmp(mode,"cancel-vhd")==0;
    if (eof_cancel) g_assert_cmpint(pipe2(control,O_CLOEXEC),==,0);
    pid_t runner=fork(); g_assert_cmpint(runner,>=,0);
    if (!runner) {
        alarm(10); cancelled=0; decompressor_pid=-1;
        if (prctl(PR_SET_CHILD_SUBREAPER,1) != 0 || install_signal_handlers() != 0) _exit(10);
        g_setenv("PATH",dir,TRUE); g_setenv("TUXFLASH_META_MODE",mode,TRUE); g_setenv("TUXFLASH_META_DIR",dir,TRUE);
        if (eof_cancel) { close(control[1]); if(start_cancellation_channel(control[0])!=0)_exit(11);close(control[0]); }
        if (strcmp(mode,"closed")==0) { close(0);close(1); }
        gboolean good=FALSE;
        if (strcmp(mode,"cancel-vhd")==0 || strcmp(mode,"limit-vhd")==0) {
            errno=0; uint64_t size=virtual_disk_size("/test-only/disk.vhd"); int saved=errno;
            good=size==0 && saved==(eof_cancel ? ECANCELED : EFBIG);
        } else {
            GError *error=NULL;
            char *language=windows_image_default_language("/test-only/windows.iso",1,&error);
            if (strcmp(mode,"streams")==0 || strcmp(mode,"closed")==0)
                good=language && strcmp(language,"pt-BR")==0 && !error;
            else if (strcmp(mode,"cancel-wim")==0)
                good=!language && g_error_matches(error,G_IO_ERROR,G_IO_ERROR_CANCELLED) && cancelled;
            else if (strcmp(mode,"limit-wim")==0)
                good=!language && g_error_matches(error,G_IO_ERROR,g_io_error_from_errno(EFBIG));
            else if (strcmp(mode,"failed-pipe")==0)
                good=!language && g_error_matches(error,G_IO_ERROR,g_io_error_from_errno(EIO)) && !cancelled;
            else good=!language && error!=NULL;
            g_free(language); g_clear_error(&error);
        }
        good=good && decompressor_pid==-1;
        if (strcmp(mode,"failed-pipe")==0) {
            char *text=NULL;long leader,leaf;int status=0;
            if (!g_file_get_contents(pids,&text,NULL,NULL) || sscanf(text,"%ld %ld",&leader,&leaf)!=2 || leaf<=1)_exit(12);
            gint64 until=g_get_monotonic_time()+500000;pid_t done;
            do {done=waitpid((pid_t)leaf,&status,WNOHANG);if(done==0)usleep(1000);}while(done==0&&g_get_monotonic_time()<until);
            good=good && done==leaf && WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL;
            if (done==0)reap_owned((pid_t)leaf);g_free(text);
        }
        _exit(good?0:15);
    }
    if (eof_cancel)close(control[0]);
    gint64 ready_until=g_get_monotonic_time()+3*G_USEC_PER_SEC;
    while(access(ready,F_OK)!=0 && g_get_monotonic_time()<ready_until)usleep(1000);
    gboolean ready_ok=access(ready,F_OK)==0;
    if (eof_cancel)close(control[1]);
    else if (strcmp(mode,"cancel-wim")==0 && ready_ok)kill(runner,SIGTERM);
    gint64 until=g_get_monotonic_time()+5*G_USEC_PER_SEC;int status=0;pid_t done;
    do {done=waitpid(runner,&status,WNOHANG);if(done==0)usleep(1000);}while(done==0&&g_get_monotonic_time()<until);
    gboolean timeout=done==0;
    if (timeout) {kill(runner,SIGKILL);wait_child(runner,&status);}
    char *record=NULL;
    if(g_file_get_contents(pids,&record,NULL,NULL)){long leader,leaf;if(sscanf(record,"%ld %ld",&leader,&leaf)==2){reap_owned((pid_t)leader);reap_owned((pid_t)leaf);}g_free(record);}
    /* No other children belong to this standalone test executable. */
    g_autofree char *children_path=g_strdup_printf("/proc/self/task/%ld/children",(long)getpid());
    char *children=NULL;
    if(g_file_get_contents(children_path,&children,NULL,NULL)){
        char **ids=g_strsplit(children," ",-1);for(char **it=ids;*it;it++)reap_owned((pid_t)g_ascii_strtoll(*it,NULL,10));g_strfreev(ids);g_free(children);
    }
    unlink(wim);unlink(qemu);unlink(ready);unlink(pids);rmdir(dir);
    g_assert_true(ready_ok);g_assert_false(timeout);g_assert_true(WIFEXITED(status));g_assert_cmpint(WEXITSTATUS(status),==,0);
}
static void streams(void){metadata_case("streams");}
static void cancel_wim(void){metadata_case("cancel-wim");}
static void cancel_vhd(void){metadata_case("cancel-vhd");}
static void limit_wim(void){metadata_case("limit-wim");}
static void limit_vhd(void){metadata_case("limit-vhd");}
static void failed_pipe(void){metadata_case("failed-pipe");}
static void invalid_bytes(void){metadata_case("invalid");metadata_case("nul");}
static void closed_stdio(void){metadata_case("closed");}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--metadata-fixture")==0)return metadata_fixture();
    g_test_init(&argc,&argv,NULL);
    g_test_add_func("/metadata/streams",streams);
    g_test_add_func("/metadata/cancel-wim",cancel_wim);
    g_test_add_func("/metadata/cancel-vhd",cancel_vhd);
    g_test_add_func("/metadata/limit-wim",limit_wim);
    g_test_add_func("/metadata/limit-vhd",limit_vhd);
    g_test_add_func("/metadata/failed-open-pipe",failed_pipe);
    g_test_add_func("/metadata/invalid-bytes",invalid_bytes);
    g_test_add_func("/metadata/closed-stdio",closed_stdio);
    return g_test_run();
}
