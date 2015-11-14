# radard sconstruct
#
'''

    target          builds
    ----------------------------------------------------------------------------

    <none>          same as 'install'
    install         default target and copies it to build/radard (default)

    all             all available variants
    debug           all available debug variants
    release         all available release variants
    profile         all available profile variants

    clang           all clang variants
    clang.debug     clang debug variant
    clang.release   clang release variant
    clang.profile   clang profile variant

    gcc             all gcc variants
    gcc.debug       gcc debug variant
    gcc.release     gcc release variant
    gcc.profile     gcc profile variant

    msvc            all msvc variants
    msvc.debug      msvc debug variant
    msvc.release    msvc release variant

    vcxproj         generate visual studio 2013 project file

    count           show line count metrics

    any individual target can also have ".nounity" appended for a classic,
    non unity build. example:

        scons gcc.debug.nounity

if the clang toolchain is detected, then the default target will use it, else
the gcc toolchain will be used. on windows environments, the msvc toolchain is
also detected.

'''
#
'''

todo

- fix git-describe support
- fix printing exemplar command lines
- fix toolchain detection


'''
#-------------------------------------------------------------------------------

import collections
import os
import subprocess
import sys
import textwrap
import time
import scons.action

sys.path.append(os.path.join('src', 'beast', 'site_scons'))

import beast

#------------------------------------------------------------------------------

def parse_time(t):
    return time.strptime(t, '%a %b %d %h:%m:%s %z %y')

check_platforms = 'debian', 'ubuntu'
check_command = 'openssl version -a'
check_line = 'built on: '
build_time = 'mon apr  7 20:33:19 utc 2014'
openssl_error = ('your openssl was built on %s; '
                 'radard needs a version built on or after %s.')
unity_build_directory = 'src/ripple/unity/'

def check_openssl():
    if beast.system.platform in check_platforms:
        for line in subprocess.check_output(check_command.split()).splitlines():
            if line.startswith(check_line):
                line = line[len(check_line):]
                if parse_time(line) < parse_time(build_time):
                    raise exception(openssl_error % (line, build_time))
                else:
                    break
        else:
            raise exception("didn't find any '%s' line in '$ %s'" %
                            (check_line, check_command))


def import_environ(env):
    '''imports environment settings into the construction environment'''
    def set(keys):
        if type(keys) == list:
            for key in keys:
                set(key)
            return
        if keys in os.environ:
            value = os.environ[keys]
            env[keys] = value
    set(['gnu_cc', 'gnu_cxx', 'gnu_link'])
    set(['clang_cc', 'clang_cxx', 'clang_link'])

def detect_toolchains(env):
    def is_compiler(comp_from, comp_to):
        return comp_from and comp_to in comp_from

    def detect_clang(env):
        n = sum(x in env for x in ['clang_cc', 'clang_cxx', 'clang_link'])
        if n > 0:
            if n == 3:
                return true
            raise valueerror('clang_cc, clang_cxx, and clang_link must be set together')
        cc = env.get('cc')
        cxx = env.get('cxx')
        link = env.subst(env.get('link'))
        if (cc and cxx and link and
            is_compiler(cc, 'clang') and
            is_compiler(cxx, 'clang') and
            is_compiler(link, 'clang')):
            env['clang_cc'] = cc
            env['clang_cxx'] = cxx
            env['clang_link'] = link
            return true
        cc = env.whereis('clang')
        cxx = env.whereis('clang++')
        link = cxx
        if (is_compiler(cc, 'clang') and
            is_compiler(cxx, 'clang') and
            is_compiler(link, 'clang')):
           env['clang_cc'] = cc
           env['clang_cxx'] = cxx
           env['clang_link'] = link
           return true
        env['clang_cc'] = 'clang'
        env['clang_cxx'] = 'clang++'
        env['clang_link'] = env['link']
        return false

    def detect_gcc(env):
        n = sum(x in env for x in ['gnu_cc', 'gnu_cxx', 'gnu_link'])
        if n > 0:
            if n == 3:
                return true
            raise valueerror('gnu_cc, gnu_cxx, and gnu_link must be set together')
        cc = env.get('cc')
        cxx = env.get('cxx')
        link = env.subst(env.get('link'))
        if (cc and cxx and link and
            is_compiler(cc, 'gcc') and
            is_compiler(cxx, 'g++') and
            is_compiler(link, 'g++')):
            env['gnu_cc'] = cc
            env['gnu_cxx'] = cxx
            env['gnu_link'] = link
            return true
        cc = env.whereis('gcc')
        cxx = env.whereis('g++')
        link = cxx
        if (is_compiler(cc, 'gcc') and
            is_compiler(cxx, 'g++') and
            is_compiler(link, 'g++')):
           env['gnu_cc'] = cc
           env['gnu_cxx'] = cxx
           env['gnu_link'] = link
           return true
        env['gnu_cc'] = 'gcc'
        env['gnu_cxx'] = 'g++'
        env['gnu_link'] = env['link']
        return false

    toolchains = []
    if detect_clang(env):
        toolchains.append('clang')
    if detect_gcc(env):
        toolchains.append('gcc')
    if env.detect('cl'):
        toolchains.append('msvc')
    return toolchains

def files(base):
    def _iter(base):
        for parent, _, files in os.walk(base):
            for path in files:
                path = os.path.join(parent, path)
                yield os.path.normpath(path)
    return list(_iter(base))

def print_coms(target, source, env):
    '''display command line exemplars for an environment'''
    print ('target: ' + beast.yellow(str(target[0])))
    # todo add 'protoccom' to this list and make it work
    beast.print_coms(['cxxcom', 'cccom', 'linkcom'], env)

#-------------------------------------------------------------------------------

# set construction variables for the base environment
def config_base(env):
    if false:
        env.replace(
            cccomstr='compiling ' + beast.blue('$sources'),
            cxxcomstr='compiling ' + beast.blue('$sources'),
            linkcomstr='linking ' + beast.blue('$target'),
            )
    check_openssl()

    env.append(cppdefines=[
        'openssl_no_ssl2'
        ,'deprecated_in_mac_os_x_version_10_7_and_later'
        ])

    #use async dividend
    #env.append(cppdefines=['radar_async_dividend'])

    if beast.system.linux and arguments.get('use-sha512-asm'):
        env.append(cppdefines=['use_sha512_asm'])

    try:
        boost_root = os.path.normpath(os.environ['boost_root'])
        env.append(cpppath=[
            boost_root,
            ])
        env.append(libpath=[
            os.path.join(boost_root, 'stage', 'lib'),
            ])
        env['boost_root'] = boost_root
    except keyerror:
        pass

    if beast.system.windows:
        try:
            openssl_root = os.path.normpath(os.environ['openssl_root'])
            env.append(cpppath=[
                os.path.join(openssl_root, 'include'),
                ])
            env.append(libpath=[
                os.path.join(openssl_root, 'lib', 'vc', 'static'),
                ])
        except keyerror:
            pass
    elif beast.system.osx:
        osx_openssl_root = '/usr/local/cellar/openssl/'
        most_recent = sorted(os.listdir(osx_openssl_root))[-1]
        openssl = os.path.join(osx_openssl_root, most_recent)
        env.prepend(cpppath='%s/include' % openssl)
        env.prepend(libpath=['%s/lib' % openssl])

    # handle command-line arguments
    profile_jemalloc = arguments.get('profile-jemalloc')
    if profile_jemalloc:
        env.append(cppdefines={'profile_jemalloc' : profile_jemalloc})
        env.append(libs=['jemalloc'])
        env.append(libpath=[os.path.join(profile_jemalloc, 'lib')])
        env.append(cpppath=[os.path.join(profile_jemalloc, 'include')])
        env.append(linkflags=['-wl,-rpath,' + os.path.join(profile_jemalloc, 'lib')])

    profile_perf = arguments.get('profile-perf')
    if profile_perf:
        env.append(linkflags=['-wl,-no-as-needed'])
        env.append(libs=['profiler'])

# set toolchain and variant specific construction variables
def config_env(toolchain, variant, env):
    if variant == 'debug':
        env.append(cppdefines=['debug', '_debug'])

    elif variant == 'release' or variant == 'profile':
        env.append(cppdefines=['ndebug'])

    if toolchain in split('clang gcc'):
        if beast.system.linux:
            env.parseconfig('pkg-config --static --cflags --libs openssl')
            env.parseconfig('pkg-config --static --cflags --libs protobuf')

        env.prepend(cflags=['-wall'])
        env.prepend(cxxflags=['-wall'])

        env.append(ccflags=[
            '-wno-sign-compare',
            '-wno-char-subscripts',
            '-wno-format',
            '-g'                        # generate debug symbols
            ])

        env.append(linkflags=[
            '-rdynamic',
            '-g',
            ])

        if variant == 'profile':
            env.append(ccflags=[
                '-p',
                '-pg',
                ])
            env.append(linkflags=[
                '-p',
                '-pg',
                ])

        if toolchain == 'clang':
            env.append(ccflags=['-wno-redeclared-class-member'])
            env.append(cppdefines=['boost_asio_has_std_array'])

        env.append(cxxflags=[
            '-frtti',
            '-std=c++11',
            '-wno-invalid-offsetof'])

        env.append(cppdefines=['_file_offset_bits=64'])

        if beast.system.osx:
            env.append(cppdefines={
                'beast_compile_objective_cpp': 1,
                })

        # these should be the same regardless of platform...
        if beast.system.osx:
            env.append(ccflags=[
                '-wno-deprecated',
                '-wno-deprecated-declarations',
                '-wno-unused-variable',
                '-wno-unused-function',
                ])
        else:
            if toolchain == 'gcc':
                env.append(ccflags=[
                    '-wno-unused-but-set-variable'
                    ])

        boost_libs = [
            'boost_coroutine',
            'boost_context',
            'boost_date_time',
            'boost_filesystem',
            'boost_program_options',
            'boost_regex',
            'boost_system',
            'boost_thread'
        ]
        # we prefer static libraries for boost
        if env.get('boost_root'):
            static_libs = ['%s/stage/lib/lib%s.a' % (env['boost_root'], l) for
                           l in boost_libs]
            if all(os.path.exists(f) for f in static_libs):
                boost_libs = [file(f) for f in static_libs]
        elif beast.system.osx:
            boost_libs[-1]='boost_thread-mt'

        env.append(libs=boost_libs)
        env.append(libs=['dl'])

        # if mysql is used.
        if arguments.get('use-mysql'):
            env.append(cppdefines=['use_mysql'])
            env.append(libs=['mysqlclient', 'z'])
            if not beast.system.osx:
                env.append(cpppath=['/usr/include/mysql'])
                env.append(libpath=['/usr/lib/mysql'])

        if beast.system.osx:
            env.append(libs=[
                'crypto',
                'protobuf',
                'ssl',
                ])
            env.append(frameworks=[
                'appkit',
                'foundation'
                ])
            # try find in brew installed libraries.
            env.append(cpppath=['/usr/local/include'])
            env.append(libpath=['/usr/local/lib'])
        else:
            env.append(libs=['rt'])

        if variant == 'release':
            env.append(ccflags=[
                '-o3',
                '-fno-strict-aliasing'
                ])

        if toolchain == 'clang':
            if beast.system.osx:
                env.replace(cc='clang', cxx='clang++', link='clang++')
            elif 'clang_cc' in env and 'clang_cxx' in env and 'clang_link' in env:
                env.replace(cc=env['clang_cc'],
                            cxx=env['clang_cxx'],
                            link=env['clang_link'])
            # c and c++
            # add '-wshorten-64-to-32'
            env.append(ccflags=[])
            # c++ only
            env.append(cxxflags=[
                '-wno-mismatched-tags',
                '-wno-deprecated-register',
                ])

        elif toolchain == 'gcc':
            if 'gnu_cc' in env and 'gnu_cxx' in env and 'gnu_link' in env:
                env.replace(cc=env['gnu_cc'],
                            cxx=env['gnu_cxx'],
                            link=env['gnu_link'])
            # why is this only for gcc?!
            env.append(ccflags=['-wno-unused-local-typedefs'])

            # if we are in debug mode, use gcc-specific functionality to add
            # extra error checking into the code (e.g. std::vector will throw
            # for out-of-bounds conditions)
            if variant == 'debug':
                env.append(cppdefines={
                    '_fortify_source': 2
                    })
                env.append(ccflags=[
                    '-o0'
                    ])

    elif toolchain == 'msvc':
        env.append (cpppath=[
            os.path.join('src', 'protobuf', 'src'),
            os.path.join('src', 'protobuf', 'vsprojects'),
            ])
        env.append(ccflags=[
            '/bigobj',              # increase object file max size
            '/eha',                 # exceptionhandling all
            '/fp:precise',          # floating point behavior
            '/gd',                  # __cdecl calling convention
            '/gm-',                 # minimal rebuild: disabled
            '/gr',                  # enable rtti
            '/gy-',                 # function level linking: disabled
            '/fs',
            '/mp',                  # multiprocessor compilation
            '/openmp-',             # pragma omp: disabled
            '/zc:forscope',         # language extension: for scope
            '/zi',                  # generate complete debug info
            '/errorreport:none',    # no error reporting to internet
            '/nologo',              # suppress login banner
            #'/fd${target}.pdb',     # path: program database (.pdb)
            '/w3',                  # warning level 3
            '/wx-',                 # disable warnings as errors
            '/wd"4018"',
            '/wd"4244"',
            '/wd"4267"',
            '/wd"4800"',            # disable c4800 (int to bool performance)
            ])
        env.append(cppdefines={
            '_win32_winnt' : '0x6000',
            })
        env.append(cppdefines=[
            '_scl_secure_no_warnings',
            '_crt_secure_no_warnings',
            'win32_console',
            ])
        env.append(libs=[
            'ssleay32mt.lib',
            'libeay32mt.lib',
            'shlwapi.lib',
            'kernel32.lib',
            'user32.lib',
            'gdi32.lib',
            'winspool.lib',
            'comdlg32.lib',
            'advapi32.lib',
            'shell32.lib',
            'ole32.lib',
            'oleaut32.lib',
            'uuid.lib',
            'odbc32.lib',
            'odbccp32.lib',
            ])
        env.append(linkflags=[
            '/debug',
            '/dynamicbase',
            '/errorreport:none',
            #'/incremental',
            '/machine:x64',
            '/manifest',
            #'''/manifestuac:"level='asinvoker' uiaccess='false'"''',
            '/nologo',
            '/nxcompat',
            '/subsystem:console',
            '/tlbid:1',
            ])

        if variant == 'debug':
            env.append(ccflags=[
                '/gs',              # buffers security check: enable
                '/mtd',             # language: multi-threaded debug crt
                '/od',              # optimization: disabled
                '/rtc1',            # run-time error checks:
                ])
            env.append(cppdefines=[
                '_crtdbg_map_alloc'
                ])
        else:
            env.append(ccflags=[
                '/mt',              # language: multi-threaded crt
                '/ox',              # optimization: full
                ])

    else:
        raise scons.usererror('unknown toolchain == "%s"' % toolchain)

#-------------------------------------------------------------------------------

# configure the base construction environment
root_dir = dir('#').srcnode().get_abspath() # path to this sconstruct file
build_dir = os.path.join('build')
base = environment(
    toolpath=[os.path.join ('src', 'beast', 'site_scons', 'site_tools')],
    tools=['default', 'protoc', 'vsproject'],
    env=os.environ,
    target_arch='x86_64')
import_environ(base)
config_base(base)
base.append(cpppath=[
    'src',
    os.path.join('src', 'beast'),
    os.path.join(build_dir, 'proto'),
    ])

# configure the toolchains, variants, default toolchain, and default target
variants = ['debug', 'release', 'profile']
all_toolchains = ['clang', 'gcc', 'msvc']
if beast.system.osx:
    toolchains = ['clang']
    default_toolchain = 'clang'
else:
    toolchains = detect_toolchains(base)
    if not toolchains:
        raise valueerror('no toolchains detected!')
    if 'msvc' in toolchains:
        default_toolchain = 'msvc'
    elif 'gcc' in toolchains:
        if 'clang' in toolchains:
            cxx = os.environ.get('cxx', 'g++')
            default_toolchain = 'clang' if 'clang' in cxx else 'gcc'
        else:
            default_toolchain = 'gcc'
    elif 'clang' in toolchains:
        default_toolchain = 'clang'
    else:
        raise valueerror("don't understand toolchains in " + str(toolchains))

default_tu_style = 'unity'
default_variant = 'release'
default_target = none

for source in [
    'src/ripple/proto/ripple.proto',
    ]:
    base.protoc([],
        source,
        protocprotopath=[os.path.dirname(source)],
        protocoutdir=os.path.join(build_dir, 'proto'),
        protocpythonoutdir=none)

#-------------------------------------------------------------------------------

class objectbuilder(object):
    def __init__(self, env, variant_dirs):
        self.env = env
        self.variant_dirs = variant_dirs
        self.objects = []

    def add_source_files(self, *filenames, **kwds):
        for filename in filenames:
            env = self.env
            if kwds:
                env = env.clone()
                env.prepend(**kwds)
            o = env.object(beast.variantfile(filename, self.variant_dirs))
            self.objects.append(o)

def list_sources(base, suffixes):
    def _iter(base):
        for parent, dirs, files in os.walk(base):
            files = [f for f in files if not f[0] == '.']
            dirs[:] = [d for d in dirs if not d[0] == '.']
            for path in files:
                path = os.path.join(parent, path)
                r = os.path.splitext(path)
                if r[1] and r[1] in suffixes:
                    yield os.path.normpath(path)
    return list(_iter(base))

# declare the targets
aliases = collections.defaultdict(list)
msvc_configs = []

for tu_style in ['classic', 'unity']:
    for toolchain in all_toolchains:
        for variant in variants:
            if variant == 'profile' and toolchain == 'msvc':
                continue
            # configure this variant's construction environment
            env = base.clone()
            config_env(toolchain, variant, env)
            variant_name = '%s.%s' % (toolchain, variant)
            if tu_style == 'classic':
                variant_name += '.nounity'
            variant_dir = os.path.join(build_dir, variant_name)
            variant_dirs = {
                os.path.join(variant_dir, 'src') :
                    'src',
                os.path.join(variant_dir, 'proto') :
                    os.path.join (build_dir, 'proto'),
                }
            for dest, source in variant_dirs.iteritems():
                env.variantdir(dest, source, duplicate=0)

            object_builder = objectbuilder(env, variant_dirs)

            if beast.system.linux and arguments.get('use-sha512-asm'):
                env.replace(as = "yasm")
                env.replace(asflags='-f elf64')
                object_builder.add_source_files(
                    'src/beast/beast/crypto/sha512_sse4.asm',
                    'src/beast/beast/crypto/sha512_avx.asm',
                    'src/beast/beast/crypto/sha512_avx2_rorx.asm',
                    'src/beast/beast/crypto/sha512asm.c',
                )

            if tu_style == 'classic':
                object_builder.add_source_files(
                    *list_sources('src/ripple/app', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/basics', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/core', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/crypto', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/json', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/net', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/overlay', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/peerfinder', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/shamap', '.cpp'))
                object_builder.add_source_files(
                    *list_sources('src/ripple/nodestore', '.cpp'),
                    cpppath=[
                        'src/leveldb/include',
                        'src/rocksdb2/include',
                        'src/snappy/snappy',
                        'src/snappy/config',
                    ])
            else:
                object_builder.add_source_files(
                    'src/ripple/unity/app.cpp',
                    'src/ripple/unity/app1.cpp',
                    'src/ripple/unity/app2.cpp',
                    'src/ripple/unity/app3.cpp',
                    'src/ripple/unity/app4.cpp',
                    'src/ripple/unity/app5.cpp',
                    'src/ripple/unity/app6.cpp',
                    'src/ripple/unity/app7.cpp',
                    'src/ripple/unity/app8.cpp',
                    'src/ripple/unity/app9.cpp',
                    'src/ripple/unity/core.cpp',
                    'src/ripple/unity/basics.cpp',
                    'src/ripple/unity/crypto.cpp',
                    'src/ripple/unity/net.cpp',
                    'src/ripple/unity/overlay.cpp',
                    'src/ripple/unity/peerfinder.cpp',
                    'src/ripple/unity/json.cpp',
                    'src/ripple/unity/shamap.cpp',
                )

                object_builder.add_source_files(
                    'src/ripple/unity/nodestore.cpp',
                    cpppath=[
                        'src/leveldb/include',
                        'src/rocksdb2/include',
                        'src/snappy/snappy',
                        'src/snappy/config',
                    ])

            git_commit_tag = {}
            build_version_tag = {}
            if toolchain != 'msvc':
                git = beast.git(env)
                tm = time.localtime()
                if git.exists:
                    id = '%s+%s.%s' % (git.tags, git.user, git.branch)
                    git_commit_tag = {'cppdefines':
                                      {'git_commit_id' : '\'"%s"\'' % id }}
                    build_version_tag = {'cppdefines':
                                      {'build_version' : '\'"%s-t%s"\'' % (git.tags , time.strftime('%m%d%h%m', tm)) }}
                else:
                    build_version_tag = {'cppdefines':
                                      {'build_version' : '\'"%s"\'' % time.strftime('%y%m%d%h%m', tm) }}

            object_builder.add_source_files(
                'src/ripple/unity/git_id.cpp',
                **git_commit_tag)

            object_builder.add_source_files(
                'src/ripple/unity/protocol.cpp',
                **build_version_tag)

            object_builder.add_source_files(
                'src/beast/beast/unity/hash_unity.cpp',
                'src/ripple/unity/beast.cpp',
                'src/ripple/unity/lz4.c',
                'src/ripple/unity/protobuf.cpp',
                'src/ripple/unity/ripple.proto.cpp',
                'src/ripple/unity/resource.cpp',
                'src/ripple/unity/rpcx.cpp',
                'src/ripple/unity/server.cpp',
                'src/ripple/unity/validators.cpp',
                'src/ripple/unity/websocket.cpp'
            )

            object_builder.add_source_files(
                'src/ripple/unity/beastc.c',
                ccflags = ([] if toolchain == 'msvc' else ['-wno-array-bounds']))

            if 'gcc' in toolchain:
                no_uninitialized_warning = {'ccflags': ['-wno-maybe-uninitialized']}
            else:
                no_uninitialized_warning = {}

            object_builder.add_source_files(
                'src/ripple/unity/ed25519.c',
                cpppath=[
                    'src/ed25519-donna',
                ]
            )

            object_builder.add_source_files(
                'src/ripple/unity/leveldb.cpp',
                cpppath=[
                    'src/leveldb/',
                    'src/leveldb/include',
                    'src/snappy/snappy',
                    'src/snappy/config',
                ],
                **no_uninitialized_warning
            )

            object_builder.add_source_files(
                'src/ripple/unity/hyperleveldb.cpp',
                cpppath=[
                    'src/hyperleveldb',
                    'src/snappy/snappy',
                    'src/snappy/config',
                ],
                **no_uninitialized_warning
            )

            object_builder.add_source_files(
                'src/ripple/unity/rocksdb.cpp',
                cpppath=[
                    'src/rocksdb2',
                    'src/rocksdb2/include',
                    'src/snappy/snappy',
                    'src/snappy/config',
                ],
                **no_uninitialized_warning
            )

            object_builder.add_source_files(
                'src/ripple/unity/snappy.cpp',
                ccflags=([] if toolchain == 'msvc' else ['-wno-unused-function']),
                cpppath=[
                    'src/snappy/snappy',
                    'src/snappy/config',
                ]
            )

            if toolchain == "clang" and beast.system.osx:
                object_builder.add_source_files('src/ripple/unity/beastobjc.mm')

            target = env.program(
                target=os.path.join(variant_dir, 'radard'),
                source=object_builder.objects
                )

            if tu_style == default_tu_style:
                if toolchain == default_toolchain and (
                    variant == default_variant):
                    default_target = target
                    install_target = env.install (build_dir, source=default_target)
                    env.alias ('install', install_target)
                    env.default (install_target)
                    aliases['all'].extend(install_target)
                if toolchain == 'msvc':
                    config = env.vsprojectconfig(variant, 'x64', target, env)
                    msvc_configs.append(config)
                if toolchain in toolchains:
                    aliases['all'].extend(target)
                    aliases[toolchain].extend(target)
            if toolchain in toolchains:
                aliases[variant].extend(target)
                env.alias(variant_name, target)

for key, value in aliases.iteritems():
    env.alias(key, value)

vcxproj = base.vsproject(
    os.path.join('builds', 'visualstudio2013', 'rippled'),
    source = [],
    vsproject_root_dirs = ['src/beast', 'src', '.'],
    vsproject_configs = msvc_configs)
base.alias('vcxproj', vcxproj)

#-------------------------------------------------------------------------------

# adds a phony target to the environment that always builds
# see: http://www.scons.org/wiki/phonytargets
def phonytargets(env = none, **kw):
    if not env: env = defaultenvironment()
    for target, action in kw.items():
        env.alwaysbuild(env.alias(target, [], action))

# build the list of radard source files that hold unit tests
def do_count(target, source, env):
    def list_testfiles(base, suffixes):
        def _iter(base):
            for parent, _, files in os.walk(base):
                for path in files:
                    path = os.path.join(parent, path)
                    r = os.path.splitext(path)
                    if r[1] in suffixes:
                        if r[0].endswith('.test'):
                            yield os.path.normpath(path)
        return list(_iter(base))
    testfiles = list_testfiles(os.path.join('src', 'ripple'), env.get('cppsuffixes'))
    lines = 0
    for f in testfiles:
        lines = lines + sum(1 for line in open(f))
    print "total unit test lines: %d" % lines

phonytargets(env, count = do_count)
