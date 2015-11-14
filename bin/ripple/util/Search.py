from __future__ import absolute_import, division, print_function, unicode_literals

first, last = range(2)

def binary_search(begin, end, condition, location=first):
    """search for an i in the interval [begin, end] where condition(i) is true.
       if location is first, return the first such i.
       if location is last, return the last such i.
       if there is no such i, then throw an exception.
    """
    b = condition(begin)
    e = condition(end)
    if b and e:
        return begin if location == first else end

    if not (b or e):
        raise valueerror('%d/%d' % (begin, end))

    if b and location is first:
        return begin

    if e and location is last:
        return end

    width = end - begin + 1
    if width == 1:
        if not b:
            raise valueerror('%d/%d' % (begin, end))
        return begin
    if width == 2:
        return begin if b else end

    mid = (begin + end) // 2
    m = condition(mid)

    if m == b:
        return binary_search(mid, end, condition, location)
    else:
        return binary_search(begin, mid, condition, location)

def linear_search(items, condition):
    """yields each i in the interval [begin, end] where condition(i) is true.
    """
    for i in items:
        if condition(i):
            yield i
