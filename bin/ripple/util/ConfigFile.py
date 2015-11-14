from __future__ import absolute_import, division, print_function, unicode_literals

import json

"""ripple has a proprietary format for their .cfg files, so we need a reader for
them."""

def read(lines):
    sections = []
    section = []
    for line in lines:
        line = line.strip()
        if (not line) or line[0] == '#':
            continue
        if line.startswith('['):
            if section:
                sections.append(section)
                section = []
        section.append(line)
    if section:
        sections.append(section)

    result = {}
    for section in sections:
        option = section.pop(0)
        assert section, ('no value for option "%s".' % option)
        assert option.startswith('[') and option.endswith(']'), (
            'no option name in block "%s"' % p[0])
        option = option[1:-1]
        assert option not in result, 'duplicate option "%s".' % option

        subdict = {}
        items = []
        for part in section:
            if '=' in part:
                assert not items, 'dictionary mixed with list.'
                k, v = part.split('=', 1)
                assert k not in subdict, 'repeated dictionary entry ' + k
                subdict[k] = v
            else:
                assert not subdict, 'list mixed with dictionary.'
                if part.startswith('{'):
                    items.append(json.loads(part))
                else:
                    words = part.split()
                    if len(words) > 1:
                        items.append(words)
                    else:
                        items.append(part)
        if len(items) == 1:
            result[option] = items[0]
        else:
            result[option] = items or subdict
    return result
