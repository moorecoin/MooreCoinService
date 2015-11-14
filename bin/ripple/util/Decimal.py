from __future__ import absolute_import, division, print_function, unicode_literals

"""fixed point numbers."""

positions = 10
positions_shift = 10 ** positions

class decimal(object):
    def __init__(self, desc='0'):
        if isinstance(desc, int):
            self.value = desc
            return
        if desc.startswith('-'):
            sign = -1
            desc = desc[1:]
        else:
            sign = 1
        parts = desc.split('.')
        if len(parts) == 1:
            parts.append('0')
        elif len(parts) > 2:
            raise exception('too many decimals in "%s"' % desc)
        number, decimal = parts
        # fix the number of positions.
        decimal = (decimal + positions * '0')[:positions]
        self.value = sign * int(number + decimal)

    def accumulate(self, item):
        if not isinstance(item, decimal):
            item = decimal(item)
        self.value += item.value

    def __str__(self):
        if self.value >= 0:
            sign = ''
            value = self.value
        else:
            sign = '-'
            value = -self.value
        number = value // positions_shift
        decimal = (value % positions_shift) * positions_shift

        if decimal:
            return '%s%s.%s' % (sign, number, str(decimal).rstrip('0'))
        else:
            return '%s%s' % (sign, number)
