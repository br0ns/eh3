import re
import sys

ERROR_SHOW_LINES = 3

def show_error(input, espan, msg):
    if espan is None:
        ebeg = eend = None
    else:
        if isinstance(espan, (tuple, list)):
            ebeg, eend = espan
        else:
            ebeg = espan
            eend = None
        eend = eend or ebeg + 1

    # First line of output from `cpp` should be (is?) `# 1 "FILENAME"`
    m = re.match(r'# 1 "(.+)"', input)
    path = m.group(1).decode('string_escape')

    if not ebeg:
        err = '%s in %s' % (msg, path)
        print >>sys.stderr, err
        return

    paths = [path]
    linenos = []
    lineno = 1
    pos = 0
    for line in input.splitlines(True):
        if pos + len(line) > ebeg:
            linenos.append(lineno)
            # 1-indexed
            column = ebeg - pos + 1
            break
        pos += len(line)

        # Match line marker
        m = re.match(r'# ([0-9]+) "(.+)"((?: [1234])*)', line)
        if not m:
            lineno += 1

        else:
            newlineno, path, flags = m.groups()
            newlineno = int(newlineno)
            path = path.decode('string_escape')
            flags = map(int, flags.split())

            if 1 in flags:
                # New file
                paths.append(path)
                linenos.append(lineno)
            elif 2 in flags:
                # Return to previous file
                paths.pop()
                linenos.pop()

            lineno = newlineno

    err = msg + ':\n'
    prefix = '   '
    for path, lineno in zip(paths, linenos):
        err += '%s %s:%d\n' % (prefix, path, lineno)
        prefix = '...'.rjust(len(prefix))

    lines = file(path, 'r').read().splitlines()[
        max(0, lineno - ERROR_SHOW_LINES) : lineno]


    padding = 2
    fmt = '%%%dd) %%s' % len(str(lineno))
    for i in xrange(len(lines)):
        lines[i] = fmt % (i + lineno - len(lines) + 1, lines[i])

    # if lineno > ERROR_SHOW_LINES:
    #         lines.insert(0, '...')

    for line in lines:
        err += line + '\n'

    if eend - ebeg > len(lines[-1]) - column + 1:
        emark = '^' * (len(lines[-1]) - column + 1 - padding) + '...'
    else:
        emark = '^' * (eend - ebeg)

    err += ' ' * (column - 1 + len(str(lineno)) + padding) + emark
    print >>sys.stderr, err
