# Meson build file
#
# SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
# SPDX-License-Identifier: MIT

project('curlws', 'c',
        version: '1.0.3',
        license: ['Apache-2.0', 'BSD-2-Clause', 'MIT'],
        default_options: ['c_std=c99',
                          'b_coverage=true'])

if not meson.is_subproject()
  # We want strict flags
  warning_level = 3   # -Wall
  werror = true
endif

################################################################################
# Common variable definitions
################################################################################

inc_base = 'include/'+meson.project_name()

################################################################################
# Generate the version header file
################################################################################

subdir('include/'+meson.project_name())

################################################################################
# Define the dependencies
################################################################################
curl_dep             = dependency('libcurl',       version: '>=7.50.2')
libtrower_base64_dep = dependency('trower-base64', version: '>=1.2.7')

all_dep = [curl_dep, libtrower_base64_dep]

if 'openssl' == get_option('sha1-provider')
  openssl_dep = dependency('openssl', version: '>=1.1.1')
  all_dep += openssl_dep
endif

################################################################################
# Define the libraries
################################################################################

inc = include_directories(['include'])

install_headers([inc_base+'/curlws.h', ver_h], subdir: meson.project_name())

sources = ['src/cb.c',
           'src/curlws.c',
           'src/data_block_sender.c',
           'src/frame.c',
           'src/frame_senders.c',
           'src/handlers.c',
           'src/header.c',
           'src/memory.c',
           'src/random.c',
           'src/receive.c',
           'src/send.c',
           'src/utf8.c',
           'src/utils.c',
           'src/verbose.c',
           'src/ws.c']

if 'openssl' == get_option('sha1-provider')
  sha_files = 'src/sha1_openssl.c'
elif 'internal' == get_option('sha-provider')
  sha_files += [ 'src/sha1_openssl.c', 'src/sha1/sha1.c' ]
else
  error('Invalid sha1-provider option.')
endif

sources += sha_files

curlws = library(meson.project_name(),
                 sources,
                 include_directories: inc,
                 dependencies: [all_dep],
                 install: true)

################################################################################
# Define the examples
################################################################################
if not meson.is_subproject()
  executable('cli', ['examples/cli/cli.c'], 
             include_directories: inc,
             dependencies: [all_dep],
             link_with: [curlws])
endif

################################################################################
# Define the tests
################################################################################

if not meson.is_subproject()
  test_args = ['-fprofile-arcs', '-g', '-ftest-coverage', '-O0']
  cunit_dep = dependency('cunit')

  # openssl version

  # Work through the non-autobahn tests
  tests = {
    'test_frame':          { 'srcs': [ 'tests/test_frame.c', 'src/frame.c' ] },
    'test_memory':         { 'srcs': [ 'tests/test_memory.c', 'src/memory.c'] },
    'test_utils':          { 'srcs': [ 'tests/test_utils.c', 'src/utils.c' ] },
    'test_utf8':           { 'srcs': [ 'tests/test_utf8.c', 'src/utf8.c' ] },

    'test_autobahn_27':    { 'srcs': [ 'tests/test_autobahn_27.c',
                                       'src/cb.c',
                                       'src/frame.c',
                                       'src/utf8.c',
                                       'src/verbose.c',
                                       'src/ws.c' ] },

    'test_curlws':         { 'srcs': [ 'tests/test_curlws.c',
                                       'src/cb.c',
                                       'src/curlws.c',
                                       'src/handlers.c',
                                       'src/memory.c',
                                       sha_files,
                                       'src/utf8.c',
                                       'src/utils.c',
                                       'src/verbose.c',
                                       'src/ws.c' ],
                             'deps': [ all_dep ] },

    'test_handlers':       { 'srcs': [ 'tests/test_handlers.c',
                                       'src/cb.c',
                                       'src/verbose.c',
                                       'src/ws.c' ],
                             'deps': [ curl_dep ] },

    'test_header':         { 'srcs': [ 'tests/test_header.c',
                                       'src/cb.c',
                                       'src/utils.c',
                                       'src/verbose.c',
                                       'src/ws.c' ] },

    'test_receive':        { 'srcs': [ 'tests/test_receive.c',
                                       'src/cb.c',
                                       'src/frame.c',
                                       'src/utf8.c',
                                       'src/verbose.c',
                                       'src/ws.c' ] },

    'test_send':           { 'srcs': [ 'tests/test_send.c',
                                       'src/frame.c',
                                       'src/verbose.c' ] },

    'test_sha1_internal':  { 'srcs': [ 'tests/test_sha1.c',
                                       'src/sha1_internal.c',
                                       'src/sha1/sha1.c' ] },

    'test_frame_senders':  { 'srcs': [ 'tests/test_frame_senders.c',
                                       'src/frame_senders.c' ] },

    'test_data_block_senders': { 'srcs': [ 'tests/test_data_block_sender.c',
                                           'src/data_block_sender.c' ] },
  }

  foreach test, vals : tests
    if 'opt' in vals and not get_option(vals['opt'])
      message('Skipping test: \u001b[1m'+test+'\u001b[0m ('+vals['opt']+' not enabled)')
    else
      deps = [cunit_dep]
      if 'deps' in vals
      deps += vals['deps']
      endif
      test(test,
           executable(test, vals['srcs'],
                      include_directories: inc,
                      dependencies: deps,
                      install: false,
                      link_args: test_args),
           suite: ['main'])
    endif
  endforeach


  # Setup the autobahn tests
  autobahn_report = executable('autobahn_report',
                               [ 'tests/autobahn_report.c', sources ],
                               include_directories: inc,
                               dependencies: [ all_dep ],
                               install: false)

  if 'parallel' == get_option('test-autobahn')
    test('Autobahn Parallel',
         executable('autobahn_ll',
                    [ 'tests/autobahn-testee-parallel.c', sources ],
                    include_directories: inc,
                    dependencies: [ all_dep ],
                    install: false,
                    link_args: test_args),
         args: [ 'ws://localhost:9001', '1', '303' ],
         suite: 'autobahn')
  elif 'serial' == get_option('test-autobahn')
    autobahn = executable('autobahn_ser',
                          [ 'tests/autobahn-testee.c', sources ],
                          include_directories: inc,
                          dependencies: [ all_dep ],
                          install: false,
                          link_args: test_args)

    foreach i : range(10, 300, 10)
      less = i - 9
      autobahn_test_name = 'autobahn_tests_'+less.to_string()+'_'+i.to_string()
      test(autobahn_test_name, autobahn,
           is_parallel: false,
           args: [ 'ws://localhost:9001', less.to_string(), i.to_string() ],
           suite: ['autobahn'])
    endforeach

    test('autobahn_tests_300_303', autobahn,
         is_parallel: false,
         args: [ 'ws://localhost:9001', '300', '303' ],
         suite: ['autobahn'])
  endif

  # Only add these targets if we're doing autobahn tests
  if 'skip' != get_option('test-autobahn')
    run_target('autobahn_collect',
               command: [autobahn_report, 'ws://localhost:9001'])

    run_target('autobahn_validate',
               command: ['../tools/check_autobahn_results.sh'])
  endif

  # Only attach valtrind to non-autobahn tests, otherwise it takes too long
  add_test_setup('normal',
                 exclude_suites: [ 'main' ],
                 timeout_multiplier: 1)

  add_test_setup('valgrind',
                 exclude_suites: [ 'autobahn' ],
                 exe_wrapper: [ 'valgrind',
                                '--leak-check=full',
                                '--error-exitcode=1' ],
                 timeout_multiplier: 2)
endif

################################################################################
# Add the license check target
################################################################################

reuse_bin = find_program('reuse', required: false)
if reuse_bin.found()
  custom_target('license',
                command: [reuse_bin, 'lint'],
                build_always_stale: true,
                output: ['license'])
endif

curlws_dep = declare_dependency(include_directories: ['include'],
                                link_with: curlws)

if meson.version().version_compare('>=0.54.0')
  meson.override_dependency(meson.project_name(), curlws_dep)
endif
