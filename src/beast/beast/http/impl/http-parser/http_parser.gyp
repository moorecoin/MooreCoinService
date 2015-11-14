# this file is used with the gyp meta build system.
# http://code.google.com/p/gyp/
# to build try this:
#   svn co http://gyp.googlecode.com/svn/trunk gyp
#   ./gyp/gyp -f make --depth=`pwd` http_parser.gyp 
#   ./out/debug/test 
{
  'target_defaults': {
    'default_configuration': 'debug',
    'configurations': {
      # todo: hoist these out and put them somewhere common, because
      #       runtimelibrary must match across the entire project
      'debug': {
        'defines': [ 'debug', '_debug' ],
        'cflags': [ '-wall', '-wextra', '-o0', '-g', '-ftrapv' ],
        'msvs_settings': {
          'vcclcompilertool': {
            'runtimelibrary': 1, # static debug
          },
        },
      },
      'release': {
        'defines': [ 'ndebug' ],
        'cflags': [ '-wall', '-wextra', '-o3' ],
        'msvs_settings': {
          'vcclcompilertool': {
            'runtimelibrary': 0, # static release
          },
        },
      }
    },
    'msvs_settings': {
      'vcclcompilertool': {
      },
      'vclibrariantool': {
      },
      'vclinkertool': {
        'generatedebuginformation': 'true',
      },
    },
    'conditions': [
      ['os == "win"', {
        'defines': [
          'win32'
        ],
      }]
    ],
  },

  'targets': [
    {
      'target_name': 'http_parser',
      'type': 'static_library',
      'include_dirs': [ '.' ],
      'direct_dependent_settings': {
        'defines': [ 'http_parser_strict=0' ],
        'include_dirs': [ '.' ],
      },
      'defines': [ 'http_parser_strict=0' ],
      'sources': [ './http_parser.c', ],
      'conditions': [
        ['os=="win"', {
          'msvs_settings': {
            'vcclcompilertool': {
              # compile as c++. http_parser.c is actually c99, but c++ is
              # close enough in this case.
              'compileas': 2,
            },
          },
        }]
      ],
    },

    {
      'target_name': 'http_parser_strict',
      'type': 'static_library',
      'include_dirs': [ '.' ],
      'direct_dependent_settings': {
        'defines': [ 'http_parser_strict=1' ],
        'include_dirs': [ '.' ],
      },
      'defines': [ 'http_parser_strict=1' ],
      'sources': [ './http_parser.c', ],
      'conditions': [
        ['os=="win"', {
          'msvs_settings': {
            'vcclcompilertool': {
              # compile as c++. http_parser.c is actually c99, but c++ is
              # close enough in this case.
              'compileas': 2,
            },
          },
        }]
      ],
    },

    {
      'target_name': 'test-nonstrict',
      'type': 'executable',
      'dependencies': [ 'http_parser' ],
      'sources': [ 'test.c' ]
    },

    {
      'target_name': 'test-strict',
      'type': 'executable',
      'dependencies': [ 'http_parser_strict' ],
      'sources': [ 'test.c' ]
    }
  ]
}
