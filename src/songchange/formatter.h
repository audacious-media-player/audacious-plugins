#ifndef FORMATTER_H
#define FORMATTER_H

#include <libaudcore/objects.h>

struct Formatter
{
    Formatter ()
        { set ('%', "%"); }

    void set (unsigned char id, const char * value)
        { values[id] = String (value); }

    StringBuf format (const char * format) const;

private:
    String values[256];
};

#endif
