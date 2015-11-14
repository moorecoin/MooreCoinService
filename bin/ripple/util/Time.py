from __future__ import absolute_import, division, print_function, unicode_literals

import datetime

# format for human-readable dates in rippled
_date_format = '%y-%b-%d'
_time_format = '%h:%m:%s'
_datetime_format = '%s %s' % (_date_format, _time_format)

_formats = _date_format, _time_format, _datetime_format

def parse_datetime(desc):
    for fmt in _formats:
        try:
            return datetime.date.strptime(desc, fmt)
        except:
            pass
    raise valueerror("can't understand date '%s'." % date)

def format_datetime(dt):
    return dt.strftime(_datetime_format)
