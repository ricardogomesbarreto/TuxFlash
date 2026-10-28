project(
  'tuxflash',
  'c',
  version: '0.3.6',
  license: 'GPL-3.0-or-later',
  meson_version: '>= 0.63.0',
  default_options: ['c_std=c11', 'warning_level=3'],
)

gtk = dependency('gtk4', version: '>= 4.14')
adwaita = dependency('libadwaita-1', version: '>= 1.5')
soup = dependency('libsoup-3.0', version: '>= 3.4')
json_glib = dependency('json-glib-1.0', version: '>= 1.8')
hivex = dependency('hivex')
threads = dependency('threads')
gio_unix = dependency('gio-unix-2.0')


add_project_arguments(
  '-D_FILE_OFFSET_BITS=64',
  '-D_FORTIFY_SOURCE=2',
  language: 'c',
)

# Reuse the original Rufus algorithms ported in the preceding work.
rufus_includes = include_directories('../upstream/rufus/src', '../upstream/rufus/src/linux')
rufus_hash = static_library(
  'rufus-linux-core',
  '../upstream/rufus/src/hash.c',
  '../upstream/rufus/src/linux/platform.c',
  include_directories: rufus_includes,
  c_args: ['-D_GNU_SOURCE', '-DRUFUS_HASH_CORE_ONLY'],
  dependencies: [threads],
)

gnome = import('gnome')
brand_resources = gnome.compile_resources(
  'tuxflash-brand',
  'assets/tuxflash.gresource.xml',
  source_dir: 'assets',
  c_name: 'tuxflash_brand',
)

helper_install_dir = get_option('prefix') / get_option('libexecdir')

lufus = executable(
  'tuxflash',
  brand_resources,
  'src/main.c',
  'src/language.c',
  'src/volume_label.c',
  'src/cli.c',
  'src/job.c',
  'src/checksums.c',
  'src/windows_options.c',
  'src/devices.c',
  'src/downloads.c',
  'src/image_probe.c',
  'src/licenses.c',
  'src/release_notes.c',
  dependencies: [gtk, adwaita, soup, json_glib, gio_unix],
  include_directories: rufus_includes,
  link_with: rufus_hash,
  link_args: ['-pthread'],
  c_args: [
    '-DLUFUS_VERSION="' + meson.project_version() + '"',
    '-DLUFUS_HELPER_PATH="' + helper_install_dir / 'tuxflash-helper' + '"',
  ],
  install: true,
)

lufus_helper = executable(
  'tuxflash-helper',
  'src/helper.c',
  'src/volume_label.c',
  'src/progress.c',
  'src/windows_options.c',
  dependencies: [dependency('gio-2.0'), json_glib],
  install: true,
  install_dir: get_option('libexecdir'),
)

reg_import = executable(
  'lufus-reg-import',
  'tools/reg_import.c',
  dependencies: [dependency('glib-2.0'), hivex],
  install: false,
)

san_policy = executable(
  'set-windows-san-policy',
  'tools/set_windows_san_policy.c',
  dependencies: [hivex],
  install: true,
  install_dir: helper_install_dir / 'bcd-sys',
)

install_data(
  'packaging/org.tuxflash.TuxFlash.desktop',
  install_dir: get_option('datadir') / 'applications',
)
install_data(
  'packaging/org.tuxflash.TuxFlash.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo',
)
install_data(
  'packaging/org.tuxflash.TuxFlash.policy',
  install_dir: get_option('datadir') / 'polkit-1/actions',
)
install_subdir(
  'assets/icons/hicolor',
  install_dir: get_option('datadir') / 'icons',
)

test('helper-help', lufus_helper, args: ['--help'])

windows_options_test = executable(
  'test-windows-options',
  'tests/test_windows_options.c',
  'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0')],
)
test('windows-options', windows_options_test)

downloads_test = executable(
  'test-downloads',
  'tests/test_downloads.c',
  'src/downloads.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0'), soup, json_glib],
)
test('downloads', downloads_test)

progress_test = executable(
  'test-progress',
  'tests/test_progress.c',
  'src/progress.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('glib-2.0')],
  c_args: [
    '-DLUFUS_SOURCE_ROOT="' + meson.project_source_root() + '"',
  ],
)
test('progress', progress_test)

helper_durability_test = executable(
  'test-helper-durability',
  'tests/test_helper_durability.c',
  'src/volume_label.c',
  'src/progress.c',
  'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0'), json_glib],
)
test('helper-durability', helper_durability_test)

job_policy_test = executable(
  'test-job-policy',
  'tests/test_job_policy.c',
  'src/job.c',
  'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0')],
)
test('job-policy', job_policy_test)

licenses_test = executable(
  'test-licenses',
  'tests/test_licenses.c',
  'src/licenses.c',
  'src/release_notes.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('glib-2.0')],
)
test('licenses', licenses_test)

image_probe_test = executable(
  'test-image-probe',
  'tests/test_image_probe.c',
  'src/image_probe.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0')],
)
test('image-probe', image_probe_test)

checksums_test = executable(
  'test-checksums',
  'tests/test_checksums.c',
  'src/checksums.c',
  include_directories: [include_directories('src'), rufus_includes],
  dependencies: [dependency('gio-2.0'), threads],
  link_with: rufus_hash,
)
test('checksums-rufus-core', checksums_test)

helper_images_test = executable(
  'test-helper-images',
  'tests/test_helper_images.c',
  'src/volume_label.c',
  'src/progress.c',
  'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0'), json_glib],
)
test('helper-images', helper_images_test)

volume_label_test = executable(
  'test-volume-label',
  'tests/test_volume_label.c',
  'src/volume_label.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('glib-2.0')],
)
test('volume-label', volume_label_test)

helper_mount_test = executable(
  'test-helper-mount',
  'tests/test_helper_mount.c',
  'src/volume_label.c',
  'src/progress.c',
  'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0'), json_glib],
)
test('helper-mount', helper_mount_test)

# Architecture-independent boot resources and prepared BCD scripts are part of
# the installed application; the SAN-policy executable is built natively above.
install_subdir('runtime/share/lufus', install_dir: get_option('datadir'))
install_subdir('runtime/share/locale', install_dir: get_option('datadir'))
install_subdir('runtime/libexec/bcd-sys',
  install_dir: get_option('libexecdir'),
  exclude_files: ['set-windows-san-policy'],
)
install_data('../docs/legal/THIRD_PARTY_NOTICES.md', '../docs/legal/UPSTREAM-LICENSE.txt',
  install_dir: get_option('datadir') / 'doc/tuxflash')

subdir('po')

language_test = executable('test-language', 'tests/test_language.c', 'src/language.c', 'src/release_notes.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0')],
  c_args: ['-DTUXFLASH_TEST_CATALOGS="' + meson.current_build_dir() / 'po' + '"'],
)
test('language-preferences-and-catalogs', language_test)

device_safety_test = executable('test-device-safety', 'tests/test_device_safety.c',
  dependencies: [dependency('gio-2.0')],
)
test('device-safety-topology', device_safety_test)

test('bcd-generator-failure', find_program('python3'), args: files('tests/test_bcd_failure.py'))

test('launcher-delegation', find_program('python3'), args: files('tests/test_launchers.py'))

test('menu-shortcut-safety', find_program('python3'), args: files('tests/test_menu_installer.py'))

helper_tree_test = executable(
  'test-helper-tree',
  'tests/test_helper_tree.c',
  'src/volume_label.c',
  'src/progress.c',
  'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0'), json_glib],
)
test('helper-tree-internal-links', helper_tree_test)

helper_verification_test = executable(
  'test-helper-verification',
  'tests/test_helper_verification.c',
  'src/volume_label.c', 'src/progress.c', 'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0'), json_glib],
)
test('helper-verification-streams', helper_verification_test, timeout: 60)

helper_io_test = executable(
  'test-helper-io',
  'tests/test_helper_io.c',
  'src/volume_label.c',
  'src/progress.c',
  'src/windows_options.c',
  include_directories: include_directories('src'),
  dependencies: [dependency('gio-2.0'), json_glib],
)
test('helper-io', helper_io_test, timeout: 90)

# Headless GTK allocation regression; enabled when the visual test tools exist.
layout_xvfb = find_program('Xvfb', required: false)
layout_python = find_program('python3')
layout_pillow = run_command(layout_python, '-c', 'import PIL', check: false)
if layout_xvfb.found() and layout_pillow.returncode() == 0
  gui_layout_test = executable(
    'test-gui-layout', brand_resources, 'tests/test_gui_layout.c',
    'src/language.c', 'src/volume_label.c', 'src/cli.c', 'src/job.c',
    'src/checksums.c', 'src/windows_options.c', 'src/devices.c', 'src/downloads.c',
    'src/image_probe.c', 'src/licenses.c', 'src/release_notes.c',
    dependencies: [gtk, adwaita, soup, json_glib, gio_unix],
    include_directories: rufus_includes, link_with: rufus_hash,
    link_args: ['-pthread', '-Wl,--wrap=lufus_devices_enumerate'],
    c_args: ['-DLUFUS_VERSION="' + meson.project_version() + '"'],
  )
  test('gui-responsive-layout', layout_python,
    args: [files('tests/test_gui_layout.py'), '--binary', gui_layout_test,
           '--catalogs', meson.current_build_dir() / 'po',
           '--output', meson.current_build_dir() / 'gui-layout-evidence',
           '--xvfb', layout_xvfb.full_path()], timeout: 180,
    is_parallel: false)
endif

# VM-only fixture: compile when optional development headers exist; never run
# on the host. Guest interpreter and kernel marker guard accidental execution.
polkit_agent = dependency('polkit-agent-1', required: false)
if polkit_agent.found()
  executable(
    'test-gui-polkit-vm', brand_resources, 'tests/vm/polkit/test_gui.c',
    'src/language.c', 'src/volume_label.c', 'src/cli.c', 'src/job.c',
    'src/checksums.c', 'src/windows_options.c', 'src/devices.c', 'src/downloads.c',
    'src/image_probe.c', 'src/licenses.c', 'src/release_notes.c',
    dependencies: [gtk, adwaita, soup, json_glib, gio_unix, polkit_agent],
    include_directories: [rufus_includes, include_directories('src')],
    link_with: rufus_hash,
    link_args: ['-pthread', '-Wl,--dynamic-linker=/opt/tuxflash/runtime/lib/ld-linux-x86-64.so.2'],
    c_args: ['-DLUFUS_VERSION="' + meson.project_version() + '"'],
  )
endif

test('polkit-vm-harness-guards', find_program('python3'), args: files('tests/test_polkit_harness.py'))
