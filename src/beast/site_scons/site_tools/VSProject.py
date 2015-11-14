# copyright 2014 vinnie falco (vinnie.falco@gmail.com)
# portions copyright the scons foundation
# portions copyright google, inc.
# this file is part of beast

"""
a scons tool to provide a family of scons builders that
generate visual studio project files
"""

import collections
import hashlib
import io
import itertools
import ntpath
import os
import pprint
import random
import re

import scons.builder
import scons.node.fs
import scons.node
import scons.script.main
import scons.util

#-------------------------------------------------------------------------------

# adapted from msvs.py

unicodebytemarker = '\xef\xbb\xbf'

v12dspheader = """\
<?xml version="1.0" encoding="%(encoding)s"?>\r
<project defaulttargets="build" toolsversion="12.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\r
"""

v12dspprojectconfiguration = """\
    <projectconfiguration include="%(variant)s|%(platform)s">\r
      <configuration>%(variant)s</configuration>\r
      <platform>%(platform)s</platform>\r
    </projectconfiguration>\r
"""

v12dspglobals = """\
  <propertygroup label="globals">\r
    <projectguid>%(project_guid)s</projectguid>\r
    <keyword>win32proj</keyword>\r
    <rootnamespace>%(name)s</rootnamespace>\r
    <ignorewarncompileduplicatedfilename>true</ignorewarncompileduplicatedfilename>\r
  </propertygroup>\r
"""

v12dsppropertygroup = """\
  <propertygroup condition="'$(configuration)|$(platform)'=='%(variant)s|%(platform)s'" label="configuration">\r
    <characterset>multibyte</characterset>\r
    <configurationtype>application</configurationtype>\r
    <platformtoolset>v120</platformtoolset>\r
    <linkincremental>false</linkincremental>\r
    <usedebuglibraries>%(use_debug_libs)s</usedebuglibraries>\r
    <useofmfc>false</useofmfc>\r
    <wholeprogramoptimization>false</wholeprogramoptimization>\r
    <intdir>%(int_dir)s</intdir>\r
    <outdir>%(out_dir)s</outdir>\r
  </propertygroup>\r
"""

v12dspimportgroup= """\
  <importgroup condition="'$(configuration)|$(platform)'=='%(variant)s|%(platform)s'" label="propertysheets">\r
    <import project="$(userrootdir)\microsoft.cpp.$(platform).user.props" condition="exists('$(userrootdir)\microsoft.cpp.$(platform).user.props')" label="localappdataplatform" />\r
  </importgroup>\r
"""

v12dspitemdefinitiongroup= """\
  <itemdefinitiongroup condition="'$(configuration)|$(platform)'=='%(variant)s|%(platform)s'">\r
"""

v12custombuildprotoc= """\
      <filetype>document</filetype>\r
      <command condition="'$(configuration)|$(platform)'=='%(name)s'">protoc --cpp_out=%(cpp_out)s --proto_path=%%(relativedir) %%(identity)</command>\r
      <outputs condition="'$(configuration)|$(platform)'=='%(name)s'">%(base_out)s.pb.h;%(base_out)s.pb.cc</outputs>\r
      <message condition="'$(configuration)|$(platform)'=='%(name)s'">protoc --cpp_out=%(cpp_out)s --proto_path=%%(relativedir) %%(identity)</message>\r
      <linkobjects condition="'$(configuration)|$(platform)'=='%(name)s'">false</linkobjects>\r
"""

v12dspfiltersheader = (
'''<?xml version="1.0" encoding="utf-8"?>\r
<project toolsversion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\r
''')

#-------------------------------------------------------------------------------

def is_subdir(child, parent):
    '''determine if child is a subdirectory of parent'''
    return os.path.commonprefix([parent, child]) == parent

def _key(item):
    if isinstance(item, (str, unicode)):
        return ('s', item.upper(), item)
    elif isinstance(item, (int, long, float)):
        return ('n', item)
    elif isinstance(item, (list, tuple)):
        return ('l', map(_key, item))
    elif isinstance(item, dict):
        return ('d', xsorted(item.keys()), xsorted(item.values()))
    elif isinstance(item, configuration):
        return ('c', _key(item.name), _key(item.target), _key(item.variant), _key(item.platform))
    elif isinstance(item, item):
        return ('i', _key(winpath(item.path())), _key(item.is_compiled()), _key(item.builder()), _key(item.tag()), _key(item.is_excluded()))
    elif isinstance(item, scons.node.fs.file):
        return ('f', _key(item.name), _key(item.suffix))
    else:
        return ('x', item)

def xsorted(tosort, **kwargs):
    '''performs sorted in a deterministic manner.'''
    if 'key' in kwargs:
        map(kwargs['key'], tosort)
    kwargs['key'] = _key
    return sorted(tosort, **kwargs)

def itemlist(items, sep):
    if type(items) == str:  # won't work in python 3.
        return items
    def gen():
        for item in xsorted(items):
            if isinstance(item, dict):
                for k, v in xsorted(item.items()):
                    yield k + '=' + v
            elif isinstance(item, (tuple, list)):
                assert len(item) == 2, "item shoud have exactly two elements: " + str(item)
                yield '%s=%s' % tuple(item)
            else:
                yield item
            yield sep
    return ''.join(gen())

#-------------------------------------------------------------------------------

class switchconverter(object):
    '''converts command line switches to msbuild xml, using tables'''

    def __init__(self, table, booltable, retable=none):
        self.table = {}
        for key in table:
            self.table[key] = table[key]
        for key in booltable:
            value = booltable[key]
            self.table[key] = [value[0], 'true']
            self.table[key + '-'] = [value[0], 'false']
        if retable != none:
            self.retable = retable
        else:
            self.retable = []

    def getxml(self, switches, prefix = ''):
        switches = list(set(switches))      # filter dupes because on windows platforms, /nologo is added automatically to the environment.
        xml = []
        for regex, tag in self.retable:
            matches = []
            for switch in switches[:]:
                match = regex.match(switch)
                if none != match:
                    matches.append(match.group(1))
                    switches.remove(switch)
            if len(matches) > 0:
                xml.append (
                    '%s<%s>%s</%s>\r\n' % (
                        prefix, tag, ';'.join(matches), tag))
        unknown = []
        for switch in switches:
            try:
                value = self.table[switch]
                xml.append (
                    '%s<%s>%s</%s>\r\n' % (
                        prefix, value[0], value[1], value[0]))
            except:
                unknown.append(switch)
        if unknown:
            s = itemlist(unknown, ' ')
            tag = 'additionaloptions'
            xml.append('%(prefix)s<%(tag)s>%(s)s%%(%(tag)s)</%(tag)s>\r\n' % locals())
        if xml:
            return ''.join(xml)
        return ''

class clswitchconverter(switchconverter):
    def __init__(self):
        booltable = {
            '/c'            : ['keepcomments'],
            '/doc'          : ['generatexmldocumentationfiles'],
            '/fau'          : ['useunicodeforassemblerlisting'],
            '/fc'           : ['usefullpaths'],
            '/fr'           : ['browseinformation'],
            '/fr'           : ['browseinformation'],
            '/fx'           : ['expandattributedsource'],
            '/gf'           : ['stringpooling'],
            '/gl'           : ['wholeprogramoptimization'],
            '/gm'           : ['minimalrebuild'],
            '/gr'           : ['runtimetypeinfo'],
            '/gs'           : ['buffersecuritycheck'],
            '/gt'           : ['enablefibersafeoptimizations'],
            '/gy'           : ['functionlevellinking'],
            '/mp'           : ['multiprocessorcompilation'],
            '/oi'           : ['intrinsicfunctions'],
            '/oy'           : ['omitframepointers'],
            '/rtcc'         : ['smallertypecheck'],
            '/u'            : ['undefineallpreprocessordefinitions'],
            '/x'            : ['ignorestandardincludepath'],
            '/wx'           : ['treatwarningaserror'],
            '/za'           : ['disablelanguageextensions'],
            '/zl'           : ['omitdefaultlibname'],
            '/fp:except'    : ['floatingpointexceptions'],
            '/hotpatch'     : ['createhotpatchableimage'],
            '/nologo'       : ['suppressstartupbanner'],
            '/openmp'       : ['openmpsupport'],
            '/showincludes' : ['showincludes'],
            '/zc:forscope'  : ['forceconformanceinforloopscope'],
            '/zc:wchar_t'   : ['treatwchar_tasbuiltintype'],
        }
        table = {
            '/ehsc' : ['exceptionhandling', 'sync'],
            '/eha'  : ['exceptionhandling', 'async'],
            '/ehs'  : ['exceptionhandling', 'synccthrow'],
            '/fa'   : ['assembleroutput', 'assemblycode'],
            '/facs' : ['assembleroutput', 'all'],
            '/fac'  : ['assembleroutput', 'assemblyandmachinecode'],
            '/fas'  : ['assembleroutput', 'assemblyandsourcecode'],
            '/gd'   : ['callingconvention', 'cdecl'],
            '/gr'   : ['callingconvention', 'fastcall'],
            '/gz'   : ['callingconvention', 'stdcall'],
            '/mt'   : ['runtimelibrary', 'multithreaded'],
            '/mtd'  : ['runtimelibrary', 'multithreadeddebug'],
            '/md'   : ['runtimelibrary', 'multithreadeddll'],
            '/mdd'  : ['runtimelibrary', 'multithreadeddebugdll'],
            '/od'   : ['optimization', 'disabled'],
            '/o1'   : ['optimization', 'minspace'],
            '/o2'   : ['optimization', 'maxspeed'],
            '/ox'   : ['optimization', 'full'],
            '/ob1'  : ['inlinefunctionexpansion', 'onlyexplicitinline'],
            '/ob2'  : ['inlinefunctionexpansion', 'anysuitable'],
            '/ot'   : ['favorsizeorspeed', 'speed'],
            '/os'   : ['favorsizeorspeed', 'size'],
            '/rtcs' : ['basicruntimechecks', 'stackframeruntimecheck'],
            '/rtcu' : ['basicruntimechecks', 'uninitializedlocalusagecheck'],
            '/rtc1' : ['basicruntimechecks', 'enablefastchecks'],
            '/tc'   : ['compileas', 'compileasc'],
            '/tp'   : ['compileas', 'compileascpp'],
            '/w0'   : [ 'warninglevel', 'turnoffallwarnings'],
            '/w1'   : [ 'warninglevel', 'level1'],
            '/w2'   : [ 'warninglevel', 'level2'],
            '/w3'   : [ 'warninglevel', 'level3'],
            '/w4'   : [ 'warninglevel', 'level4'],
            '/wall' : [ 'warninglevel', 'enableallwarnings'],
            '/yc'   : ['precompiledheader', 'create'],
            '/yu'   : ['precompiledheader', 'use'],
            '/z7'   : ['debuginformationformat', 'oldstyle'],
            '/zi'   : ['debuginformationformat', 'programdatabase'],
            '/zi'   : ['debuginformationformat', 'editandcontinue'],
            '/zp1'  : ['structmemberalignment', '1byte'],
            '/zp2'  : ['structmemberalignment', '2bytes'],
            '/zp4'  : ['structmemberalignment', '4bytes'],
            '/zp8'  : ['structmemberalignment', '8bytes'],
            '/zp16'         : ['structmemberalignment', '16bytes'],
            '/arch:ia32'     : ['enableenhancedinstructionset', 'noextensions'],
            '/arch:sse'      : ['enableenhancedinstructionset', 'streamingsimdextensions'],
            '/arch:sse2'     : ['enableenhancedinstructionset', 'streamingsimdextensions2'],
            '/arch:avx'      : ['enableenhancedinstructionset', 'advancedvectorextensions'],
            '/clr'           : ['compileasmanaged', 'true'],
            '/clr:pure'      : ['compileasmanaged', 'pure'],
            '/clr:safe'      : ['compileasmanaged', 'safe'],
            '/clr:oldsyntax' : ['compileasmanaged', 'oldsyntax'],
            '/fp:fast'       : ['floatingpointmodel', 'fast'],
            '/fp:precise'    : ['floatingpointmodel', 'precise'],
            '/fp:strict'     : ['floatingpointmodel', 'strict'],
            '/errorreport:none'   : ['errorreporting', 'none'],
            '/errorreport:prompt' : ['errorreporting', 'prompt'],
            '/errorreport:queue'  : ['errorreporting', 'queue'],
            '/errorreport:send'   : ['errorreporting', 'send'],
        }
        retable = [
            (re.compile(r'/wd\"(\d+)\"'), 'disablespecificwarnings'),
        ]
        # ideas from google's generate your project
        '''
        _same(_compile, 'additionalincludedirectories', _folder_list)  # /i

        _same(_compile, 'preprocessordefinitions', _string_list)  # /d
        _same(_compile, 'programdatabasefilename', _file_name)  # /fd

        _same(_compile, 'additionaloptions', _string_list)
        _same(_compile, 'additionalusingdirectories', _folder_list)  # /ai
        _same(_compile, 'assemblerlistinglocation', _file_name)  # /fa
        _same(_compile, 'browseinformationfile', _file_name)
        _same(_compile, 'forcedincludefiles', _file_list)  # /fi
        _same(_compile, 'forcedusingfiles', _file_list)  # /fu
        _same(_compile, 'undefinepreprocessordefinitions', _string_list)  # /u
        _same(_compile, 'xmldocumentationfilename', _file_name)
           ''    : ['enableprefast', _boolean)  # /analyze visible='false'
        _renamed(_compile, 'objectfile', 'objectfilename', _file_name)  # /fo
        _renamed(_compile, 'precompiledheaderthrough', 'precompiledheaderfile',
                 _file_name)  # used with /yc and /yu
        _renamed(_compile, 'precompiledheaderfile', 'precompiledheaderoutputfile',
                 _file_name)  # /fp
        _convertedtoadditionaloption(_compile, 'defaultcharisunsigned', '/j')
        _msbuildonly(_compile, 'processornumber', _integer)  # the number of processors
        _msbuildonly(_compile, 'trackerlogdirectory', _folder_name)
        _msbuildonly(_compile, 'treatspecificwarningsaserrors', _string_list)  # /we
        _msbuildonly(_compile, 'preprocessoutputpath', _string)  # /fi
        '''
        switchconverter.__init__(self, table, booltable, retable)

class linkswitchconverter(switchconverter):
    def __init__(self):
        # based on code in generate your project
        booltable = {
            '/debug'                : ['generatedebuginformation'],
            '/dynamicbase'          : ['randomizedbaseaddress'],
            '/nologo'               : ['suppressstartupbanner'],
            '/nologo'               : ['suppressstartupbanner'],
        }
        table = {
            '/errorreport:none'     : ['errorreporting', 'noerrorreport'],
            '/errorreport:prompt'   : ['errorreporting', 'promptimmediately'],
            '/errorreport:queue'    : ['errorreporting', 'queuefornextlogin'],
            '/errorreport:send'     : ['errorreporting', 'senderrorreport'],
            '/machine:x86'          : ['targetmachine', 'machinex86'],
            '/machine:arm'          : ['targetmachine', 'machinearm'],
            '/machine:ebc'          : ['targetmachine', 'machineebc'],
            '/machine:ia64'         : ['targetmachine', 'machineia64'],
            '/machine:mips'         : ['targetmachine', 'machinemips'],
            '/machine:mips16'       : ['targetmachine', 'machinemips16'],
            '/machine:mipsfpu'      : ['targetmachine', 'machinemipsfpu'],
            '/machine:mipsfpu16'    : ['targetmachine', 'machinemipsfpu16'],
            '/machine:sh4'          : ['targetmachine', 'machinesh4'],
            '/machine:thumb'        : ['targetmachine', 'machinethumb'],
            '/machine:x64'          : ['targetmachine', 'machinex64'],
            '/nxcompat'             : ['dataexecutionprevention', 'true'],
            '/nxcompat:no'          : ['dataexecutionprevention', 'false'],
            '/subsystem:console'                    : ['subsystem', 'console'],
            '/subsystem:windows'                    : ['subsystem', 'windows'],
            '/subsystem:native'                     : ['subsystem', 'native'],
            '/subsystem:efi_application'            : ['subsystem', 'efi application'],
            '/subsystem:efi_boot_service_driver'    : ['subsystem', 'efi boot service driver'],
            '/subsystem:efi_rom'                    : ['subsystem', 'efi rom'],
            '/subsystem:efi_runtime_driver'         : ['subsystem', 'efi runtime'],
            '/subsystem:windowsce'                  : ['subsystem', 'windowsce'],
            '/subsystem:posix'                      : ['subsystem', 'posix'],
        }
        '''
        /tlbid:1 /manifest /manifestuac:level='asinvoker' uiaccess='false'

        _same(_link, 'allowisolation', _boolean)  # /allowisolation
        _same(_link, 'clrunmanagedcodecheck', _boolean)  # /clrunmanagedcodecheck
        _same(_link, 'delaysign', _boolean)  # /delaysign
        _same(_link, 'enableuac', _boolean)  # /manifestuac
        _same(_link, 'generatemapfile', _boolean)  # /map
        _same(_link, 'ignorealldefaultlibraries', _boolean)  # /nodefaultlib
        _same(_link, 'ignoreembeddedidl', _boolean)  # /ignoreidl
        _same(_link, 'mapexports', _boolean)  # /mapinfo:exports
        _same(_link, 'stripprivatesymbols', _file_name)  # /pdbstripped
        _same(_link, 'peruserredirection', _boolean)
        _same(_link, 'profile', _boolean)  # /profile
        _same(_link, 'registeroutput', _boolean)
        _same(_link, 'setchecksum', _boolean)  # /release
        _same(_link, 'supportunloadofdelayloadeddll', _boolean)  # /delay:unload
        
        _same(_link, 'swaprunfromcd', _boolean)  # /swaprun:cd
        _same(_link, 'turnoffassemblygeneration', _boolean)  # /noassembly
        _same(_link, 'uacuiaccess', _boolean)  # /uiaccess='true'
        _same(_link, 'enablecomdatfolding', _newly_boolean)  # /opt:icf
        _same(_link, 'fixedbaseaddress', _newly_boolean)  # /fixed
        _same(_link, 'largeaddressaware', _newly_boolean)  # /largeaddressaware
        _same(_link, 'optimizereferences', _newly_boolean)  # /opt:ref
        _same(_link, 'terminalserveraware', _newly_boolean)  # /tsaware

        _same(_link, 'additionaldependencies', _file_list)
        _same(_link, 'additionallibrarydirectories', _folder_list)  # /libpath 
        _same(_link, 'additionalmanifestdependencies', _file_list)  # /manifestdependency:
        _same(_link, 'additionaloptions', _string_list)
        _same(_link, 'addmodulenamestoassembly', _file_list)  # /assemblymodule
        _same(_link, 'assemblylinkresource', _file_list)  # /assemblylinkresource
        _same(_link, 'baseaddress', _string)  # /base
        _same(_link, 'delayloaddlls', _file_list)  # /delayload
        _same(_link, 'embedmanagedresourcefile', _file_list)  # /assemblyresource
        _same(_link, 'entrypointsymbol', _string)  # /entry
        _same(_link, 'forcesymbolreferences', _file_list)  # /include
        _same(_link, 'functionorder', _file_name)  # /order
        _same(_link, 'heapcommitsize', _string)
        _same(_link, 'heapreservesize', _string)  # /heap
        _same(_link, 'importlibrary', _file_name)  # /implib
        _same(_link, 'keycontainer', _file_name)  # /keycontainer
        _same(_link, 'keyfile', _file_name)  # /keyfile
        _same(_link, 'manifestfile', _file_name)  # /manifestfile
        _same(_link, 'mapfilename', _file_name)
        _same(_link, 'mergedidlbasefilename', _file_name)  # /idlout
        _same(_link, 'mergesections', _string)  # /merge
        _same(_link, 'midlcommandfile', _file_name)  # /midl
        _same(_link, 'moduledefinitionfile', _file_name)  # /def
        _same(_link, 'outputfile', _file_name)  # /out
        _same(_link, 'profileguideddatabase', _file_name)  # /pgd
        _same(_link, 'programdatabasefile', _file_name)  # /pdb
        _same(_link, 'stackcommitsize', _string)
        _same(_link, 'stackreservesize', _string)  # /stack
        _same(_link, 'typelibraryfile', _file_name)  # /tlbout
        _same(_link, 'typelibraryresourceid', _integer)  # /tlbid
        _same(_link, 'version', _string)  # /version


        _same(_link, 'assemblydebug',
              _enumeration(['',
                            'true',  # /assemblydebug
                            'false']))  # /assemblydebug:disable
        _same(_link, 'clrimagetype',
              _enumeration(['default',
                            'forceijwimage',  # /clrimagetype:ijw
                            'forcepureilimage',  # /switch="clrimagetype:pure
                            'forcesafeilimage']))  # /switch="clrimagetype:safe
        _same(_link, 'clrthreadattribute',
              _enumeration(['defaultthreadingattribute',  # /clrthreadattribute:none
                            'mtathreadingattribute',  # /clrthreadattribute:mta
                            'stathreadingattribute']))  # /clrthreadattribute:sta
        _same(_link, 'driver',
              _enumeration(['notset',
                            'driver',  # /driver
                            'uponly',  # /driver:uponly
                            'wdm']))  # /driver:wdm
        _same(_link, 'linktimecodegeneration',
              _enumeration(['default',
                            'uselinktimecodegeneration',  # /ltcg
                            'pginstrument',  # /ltcg:pginstrument
                            'pgoptimization',  # /ltcg:pgoptimize
                            'pgupdate']))  # /ltcg:pgupdate
        _same(_link, 'showprogress',
              _enumeration(['notset',
                            'linkverbose',  # /verbose
                            'linkverboselib'],  # /verbose:lib
                           new=['linkverboseicf',  # /verbose:icf
                                'linkverboseref',  # /verbose:ref
                                'linkverbosesafeseh',  # /verbose:safeseh
                                'linkverboseclr']))  # /verbose:clr
        _same(_link, 'uacexecutionlevel',
              _enumeration(['asinvoker',  # /level='asinvoker'
                            'highestavailable',  # /level='highestavailable'
                            'requireadministrator']))  # /level='requireadministrator'
        _same(_link, 'minimumrequiredversion', _string)
        _same(_link, 'treatlinkerwarningaserrors', _boolean)  # /wx


        # options found in msvs that have been renamed in msbuild.
        _renamed(_link, 'ignoredefaultlibrarynames', 'ignorespecificdefaultlibraries',
                 _file_list)  # /nodefaultlib
        _renamed(_link, 'resourceonlydll', 'noentrypoint', _boolean)  # /noentry
        _renamed(_link, 'swaprunfromnet', 'swaprunfromnet', _boolean)  # /swaprun:net

        _moved(_link, 'generatemanifest', '', _boolean)
        _moved(_link, 'ignoreimportlibrary', '', _boolean)
        _moved(_link, 'linkincremental', '', _newly_boolean)
        _moved(_link, 'linklibrarydependencies', 'projectreference', _boolean)
        _moved(_link, 'uselibrarydependencyinputs', 'projectreference', _boolean)

        # msvs options not found in msbuild.
        _msvsonly(_link, 'optimizeforwindows98', _newly_boolean)
        _msvsonly(_link, 'useunicoderesponsefiles', _boolean)

        # msbuild options not found in msvs.
        _msbuildonly(_link, 'buildinginide', _boolean)
        _msbuildonly(_link, 'imagehassafeexceptionhandlers', _boolean)  # /safeseh
        _msbuildonly(_link, 'linkdll', _boolean)  # /dll visible='false'
        _msbuildonly(_link, 'linkstatus', _boolean)  # /ltcg:status
        _msbuildonly(_link, 'preventdllbinding', _boolean)  # /allowbind
        _msbuildonly(_link, 'supportnobindofdelayloadeddll', _boolean)  # /delay:nobind
        _msbuildonly(_link, 'trackerlogdirectory', _folder_name)
        _msbuildonly(_link, 'msdosstubfilename', _file_name)  # /stub visible='false'
        _msbuildonly(_link, 'sectionalignment', _integer)  # /align
        _msbuildonly(_link, 'specifysectionattributes', _string)  # /section
        _msbuildonly(_link, 'forcefileoutput',
                     _enumeration([], new=['enabled',  # /force
                                           # /force:multiple
                                           'multiplydefinedsymbolonly',
                                           'undefinedsymbolonly']))  # /force:unresolved
        _msbuildonly(_link, 'createhotpatchableimage',
                     _enumeration([], new=['enabled',  # /functionpadmin
                                           'x86image',  # /functionpadmin:5
                                           'x64image',  # /functionpadmin:6
                                           'itaniumimage']))  # /functionpadmin:16
        _msbuildonly(_link, 'clrsupportlasterror',
                     _enumeration([], new=['enabled',  # /clrsupportlasterror
                                           'disabled',  # /clrsupportlasterror:no
                                           # /clrsupportlasterror:systemdll
                                           'systemdlls']))

        '''
        switchconverter.__init__(self, table, booltable)

clswitches = clswitchconverter()
linkswitches = linkswitchconverter()

#-------------------------------------------------------------------------------

# return a windows path from a native path
def winpath(path):
    drive, rest = ntpath.splitdrive(path)
    result = []
    while rest and rest != ntpath.sep:
        rest, part = ntpath.split(rest)
        result.insert(0, part)
    if rest:
        result.insert(0, rest)
    return ntpath.join(drive.upper(), *result)

def makelist(x):
    if not x:
        return []
    if type(x) is not list:
        return [x]
    return x

#-------------------------------------------------------------------------------

class configuration(object):
    def __init__(self, variant, platform, target, env):
        self.name = '%s|%s' % (variant, platform)
        self.variant = variant
        self.platform = platform
        self.target = target
        self.env = env

#-------------------------------------------------------------------------------

class item(object):
    '''represents a file item in the solution explorer'''
    def __init__(self, path, builder):
        self._path = path
        self._builder = builder
        self.node = dict()

        if builder == 'object':
            self._tag = 'clcompile'
            self._excluded = false
        elif builder == 'protoc':
            self._tag = 'custombuild'
            self._excluded = false
        else:
            ext = os.path.splitext(self._path)[1]
            if ext in ['.c', '.cc', '.cpp']:
                self._tag = 'clcompile'
                self._excluded = true
            else:
                if ext in ['.h', '.hpp', '.hxx', '.inl', '.inc']:
                    self._tag = 'clinclude'
                else:
                    self._tag = 'none'
                self._excluded = false;

    def __repr__(self):
        return '<vsproject.item "%s" %s>' % (
            self.path, self.tag, str(self.node))

    def path(self):
        return self._path

    def tag(self):
        return self._tag

    def builder(self):
        return self._builder

    def is_compiled(self):
        return self._builder == 'object'

    def is_excluded(self):
        return self._excluded

#-------------------------------------------------------------------------------

def _guid(seed, name = none):
    m = hashlib.md5()
    m.update(seed)
    if name:
        m.update(name)
    d = m.hexdigest().upper()
    guid = "{%s-%s-%s-%s-%s}" % (d[:8], d[8:12], d[12:16], d[16:20], d[20:32])
    return guid

class _projectgenerator(object):
    '''generates a project file for visual studio 2013'''

    def __init__(self, project_node, filters_node, env):
        try:
            self.configs = xsorted(env['vsproject_configs'])
        except keyerror:
            raise valueerror ('missing vsproject_configs')
        self.root_dir = os.getcwd()
        self.root_dirs = [os.path.abspath(x) for x in makelist(env['vsproject_root_dirs'])]
        self.project_dir = os.path.dirname(os.path.abspath(str(project_node)))
        self.project_node = project_node
        self.project_file = none
        self.filters_node = filters_node
        self.filters_file = none
        self.guid = _guid(os.path.basename(str(self.project_node)))
        self.builditemlist(env)

    def builditemlist(self, env):
        '''build the item set associated with the configurations'''
        items = {}
        def _walk(target, items, prefix=''):
            if os.path.isabs(str(target)):
                return
            if target.has_builder():
                builder = target.get_builder().get_name(env)
                bsources = target.get_binfo().bsources
                if builder == 'program':
                    for child in bsources:
                        _walk(child, items, prefix+'  ')
                else:
                    for child in bsources:
                        item = items.setdefault(str(child), item(str(child), builder=builder))
                        item.node[config] = target
                        _walk(child, items, prefix+'  ')
                    for child in target.children(scan=1):
                        if not os.path.isabs(str(child)):
                            item = items.setdefault(str(child), item(str(child), builder=none))
                            _walk(child, items, prefix+'  ')
        for config in self.configs:
            targets = config.target
            for target in targets:
                _walk(target, items)
        self.items = xsorted(items.values())

    def makelisttag(self, items, prefix, tag, attrs, inherit=true):
        '''builds an xml tag string from a list of items. if items is
        empty, then the returned string is empty.'''
        if not items:
            return ''
        s = '%(prefix)s<%(tag)s%(attrs)s>' % locals()
        s += ';'.join(items)
        if inherit:
            s += ';%%(%(tag)s)' % locals()
        s += '</%(tag)s>\r\n' % locals()
        return s

    def relpaths(self, paths):
        items = []
        for path in paths:
            if not os.path.isabs(path):
                items.append(winpath(os.path.relpath(path, self.project_dir)))
        return items

    def extrarelpaths(self, paths, base):
        extras = []
        for path in paths:
            if not path in base:
                extras.append(path)
        return self.relpaths(extras)

    def writeheader(self):
        global clswitches

        encoding = 'utf-8'
        project_guid = self.guid
        name = os.path.splitext(os.path.basename(str(self.project_node)))[0]

        f = self.project_file
        f.write(unicodebytemarker)
        f.write(v12dspheader % locals())
        f.write(v12dspglobals % locals())
        f.write('  <itemgroup label="projectconfigurations">\r\n')
        for config in self.configs:
            variant = config.variant
            platform = config.platform           
            f.write(v12dspprojectconfiguration % locals())
        f.write('  </itemgroup>\r\n')

        f.write('  <import project="$(vctargetspath)\microsoft.cpp.default.props" />\r\n')
        for config in self.configs:
            variant = config.variant
            platform = config.platform
            use_debug_libs = variant == 'debug'
            variant_dir = os.path.relpath(os.path.dirname(
                config.target[0].get_abspath()), self.project_dir)
            out_dir = winpath(variant_dir) + ntpath.sep
            int_dir = winpath(ntpath.join(variant_dir, 'src')) + ntpath.sep
            f.write(v12dsppropertygroup % locals())

        f.write('  <import project="$(vctargetspath)\microsoft.cpp.props" />\r\n')
        f.write('  <importgroup label="extensionsettings" />\r\n')
        for config in self.configs:
            variant = config.variant
            platform = config.platform
            f.write(v12dspimportgroup % locals())

        f.write('  <propertygroup label="usermacros" />\r\n')
        for config in self.configs:
            variant = config.variant
            platform = config.platform
            f.write(v12dspitemdefinitiongroup % locals())
            # cl options
            f.write('    <clcompile>\r\n')
            f.write(
                '      <preprocessordefinitions>%s%%(preprocessordefinitions)</preprocessordefinitions>\r\n' % (
                    itemlist(config.env['cppdefines'], ';')))
            props = ''
            props += self.makelisttag(self.relpaths(xsorted(config.env['cpppath'])),
                '      ', 'additionalincludedirectories', '', true)
            f.write(props)
            f.write(clswitches.getxml(xsorted(config.env['ccflags']), '      '))
            f.write('    </clcompile>\r\n')

            f.write('    <link>\r\n')
            props = ''
            props += self.makelisttag(xsorted(config.env['libs']),
                '      ', 'additionaldependencies', '', true)
            try:
                props += self.makelisttag(self.relpaths(xsorted(config.env['libpath'])),
                    '      ', 'additionallibrarydirectories', '', true)
            except:
                pass
            f.write(props)
            f.write(linkswitches.getxml(xsorted(config.env['linkflags']), '      '))
            f.write('    </link>\r\n')

            f.write('  </itemdefinitiongroup>\r\n')

    def writeproject(self):
        self.writeheader()

        f = self.project_file
        self.project_file.write('  <itemgroup>\r\n')
        for item in self.items:
            path = winpath(os.path.relpath(item.path(), self.project_dir))
            props = ''
            tag = item.tag()
            if item.is_excluded():
                props = '      <excludedfrombuild>true</excludedfrombuild>\r\n'
            elif item.builder() == 'object':
                props = ''
                for config, output in xsorted(item.node.items()):
                    name = config.name
                    env = output.get_build_env()
                    variant = config.variant
                    platform = config.platform
                    props += self.makelisttag(self.extrarelpaths(xsorted(env['cpppath']), config.env['cpppath']),
                        '      ', 'additionalincludedirectories',
                        ''' condition="'$(configuration)|$(platform)'=='%(variant)s|%(platform)s'"''' % locals(),
                        true)
            elif item.builder() == 'protoc':
                for config, output in xsorted(item.node.items()):
                    name = config.name
                    out_dir = os.path.relpath(os.path.dirname(str(output)), self.project_dir)
                    cpp_out = winpath(out_dir)
                    out_parts = out_dir.split(os.sep)
                    out_parts.append(os.path.splitext(os.path.basename(item.path()))[0])
                    base_out = ntpath.join(*out_parts)
                    props += v12custombuildprotoc % locals()
 
            f.write('    <%(tag)s include="%(path)s">\r\n' % locals())
            f.write(props)
            f.write('    </%(tag)s>\r\n' % locals())
        f.write('  </itemgroup>\r\n')

        f.write(
            '  <import project="$(vctargetspath)\microsoft.cpp.targets" />\r\n'
            '  <importgroup label="extensiontargets">\r\n'
            '  </importgroup>\r\n'
            '</project>\r\n')

    def writefilters(self):
        def getgroup(abspath):
            abspath = os.path.dirname(abspath)
            for d in self.root_dirs:
                common = os.path.commonprefix([abspath, d])
                if common == d:
                    return winpath(os.path.relpath(abspath, common))
            return winpath(os.path.split(abspath)[1])

        f = self.filters_file
        f.write(unicodebytemarker)
        f.write(v12dspfiltersheader)

        f.write('  <itemgroup>\r\n')
        groups = set()
        for item in self.items:
            group = getgroup(os.path.abspath(item.path()))
            while group != '':
                groups.add(group)
                group = ntpath.split(group)[0]
        for group in xsorted(groups):
            guid = _guid(self.guid, group)
            f.write(
                '    <filter include="%(group)s">\r\n'
                '      <uniqueidentifier>%(guid)s</uniqueidentifier>\r\n'
                '    </filter>\r\n' % locals())
        f.write('  </itemgroup>\r\n')

        f.write('  <itemgroup>\r\n')
        for item in self.items:
            path = os.path.abspath(item.path())
            group = getgroup(path)
            path = winpath(os.path.relpath(path, self.project_dir))
            tag = item.tag()
            f.write (
                '    <%(tag)s include="%(path)s">\r\n'
                '      <filter>%(group)s</filter>\r\n'
                '    </%(tag)s>\r\n' % locals())
        f.write('  </itemgroup>\r\n')
        f.write('</project>\r\n')

    def build(self):
        try:
            self.project_file = open(str(self.project_node), 'wb')
        except ioerror, detail:
            raise scons.errors.internalerror('unable to open "' +
                str(self.project_node) + '" for writing:' + str(detail))
        try:
            self.filters_file = open(str(self.filters_node), 'wb')
        except ioerror, detail:
            raise scons.errors.internalerror('unable to open "' +
                str(self.filters_node) + '" for writing:' + str(detail))
        self.writeproject()
        self.writefilters()
        self.project_file.close()
        self.filters_file.close()

#-------------------------------------------------------------------------------

class _solutiongenerator(object):
    def __init__(self, slnfile, projfile, env):
        pass

    def build(self):
        pass

#-------------------------------------------------------------------------------

# generate the vs2013 project
def buildproject(target, source, env):
    if env.get('auto_build_solution', 1):
        if len(target) != 3:
            raise valueerror ("unexpected len(target) != 3")
    if not env.get('auto_build_solution', 1):
        if len(target) != 2:
            raise valueerror ("unexpected len(target) != 2")

    g = _projectgenerator (target[0], target[1], env)
    g.build()

    if env.get('auto_build_solution', 1):
        g = _solutiongenerator (target[2], target[0], env)
        g.build()

def projectemitter(target, source, env):
    if len(target) != 1:
        raise valueerror ("exactly one target must be specified")

    # if source is unspecified this condition will be true
    if not source or source[0] == target[0]:
        source = []

    outputs = []
    for node in list(target):
        path = env.getbuildpath(node)
        outputs.extend([
            path + '.vcxproj',
            path + '.vcxproj.filters'])
        if env.get('auto_build_solution', 1):
            outputs.append(path + '.sln')
    return outputs, source

projectbuilder = scons.builder.builder(
    action = scons.action.action(buildproject, "building ${target}"),
    emitter = projectemitter)

def createconfig(self, variant, platform, target, env):
    return configuration(variant, platform, target, env)

def generate(env):
    '''add builders and construction variables for microsoft visual
    studio project files to an environment.'''
    try:
      env['builders']['vsproject']
    except keyerror:
      env['builders']['vsproject'] = projectbuilder
    env.addmethod(createconfig, 'vsprojectconfig')

def exists(env):
    return true
