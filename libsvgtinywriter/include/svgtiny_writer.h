#ifndef SVGTINYWRITER_H
#define SVGTINYWRITER_H

typedef enum {
	svgtinywriter_OK,
	svgtinywriter_OUT_OF_MEMORY,
	svgtinywriter_BUFFER_TOO_SMALL,
	svgtinywriter_SVG_ERROR,
} svgtinywriter_code;

struct svgtiny_diagram; // Forward declaration. #include "svgtiny.h" to manipulate the data structures.

// returns, in outLength , the length of the buffer long enough to hold the output string.
svgtinywriter_code svgtinywriter_length(const struct svgtiny_diagram* diagram, int *outLength);

// writes the SVG as a string to the output buffer,
svgtinywriter_code svgtinywriter_write(const struct svgtiny_diagram* diagram, int maximumLength, char *outputBuffer, int *outLength);


#endif  // SVGTINYWRITER_H
